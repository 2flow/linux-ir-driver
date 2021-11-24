// Copyright 2021
// Author: Florian Wimmer

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/proc_fs.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <linux/gpio.h> 
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/poll.h>


/* Tree configuration

	irdriver {
		compatible = "flow,irdevice";
		status = "okay";
		device-name = "ir_device_1";
		gpio-pin = <26>;
	};

*/

// ************ module description ***************
MODULE_LICENSE("GPL");
MODULE_AUTHOR("IR reading");
MODULE_DESCRIPTION("");
MODULE_VERSION("1.0");

#define DEVICE_NAME "flow,irdevice"
#define DRIVER_NAME "irdriver"

static const unsigned int MINOR_BASE = 0;
static const unsigned int MINOR_NUM  = 1;

static const struct of_device_id irdevice_of_match_table[] = {
	{.compatible = DEVICE_NAME,},
	{ },
};

MODULE_DEVICE_TABLE(of, irdevice_of_match_table);

// ***** Decoding steps *******
#define STATE_TYPE uint8_t

//  define decoding steps
#define STATE_START_1 0
#define STATE_BIT 2
#define STATE_BIT_HIGH 3
#define STATE_IDLE 4

#define STATE_START_2 5
#define STATE_BIT_LOW 6


// ************* device data ********************
struct ir_device_info {
	int gpio_pin;
	STATE_TYPE decode_state;
	unsigned int pin_irq_number_rising;
	uint64_t last_irq_time;
	uint32_t value;
	uint16_t counter;
	bool ir_last_state;
	uint32_t last_value;
	struct proc_dir_entry *ir_proc;
};


// ************** proc-file configuration ****************
ssize_t ir_read(struct file *file, char __user *user, size_t size, loff_t *off){

	struct ir_device_info *data;
	data=PDE_DATA(file_inode(file));
	if(!(data)){
		copy_to_user(user, 0, 4);
		return 5;
	}

	copy_to_user(user, &data->last_value, 4);
	data->last_value = 0;

	return 4;
}

static const struct proc_ops ir_proc_fops = {
	.proc_read = ir_read
};


// ************* helper functions ******************

/**
 * Checks if the timing is correct and s
 */
inline static void next_if_timing_in_range(uint64_t time_diff,
	uint64_t lower_limit, 
	uint64_t uper_limit, 
	STATE_TYPE nextState,
	struct ir_device_info * dev_info){

	if((time_diff > lower_limit) && (time_diff < uper_limit)) {
		dev_info->decode_state = nextState;
	}else{
		dev_info->decode_state = STATE_IDLE;
	}
}


// ***************** interrupt handling ****************
static void ir_pin_rising(uint64_t time_diff, struct ir_device_info * dev_info){
	switch(dev_info->decode_state){
		case STATE_START_1:{
			next_if_timing_in_range(time_diff, 4000000, 10000000, STATE_START_2, dev_info);
			break;
		}
		case STATE_BIT_LOW:{
			if((time_diff < 400000) || (dev_info->counter >= 31)){
				dev_info->last_value = dev_info->value;
				dev_info->decode_state = STATE_IDLE;
			}else if(time_diff < 827000){
				dev_info->decode_state = STATE_BIT_HIGH;
			}

			break;
		}
	}

	
}

static void ir_pin_falling(uint64_t time_diff, struct ir_device_info * dev_info){

	switch(dev_info->decode_state){
		case STATE_IDLE:{
			dev_info->decode_state = STATE_START_1;
			
			break;
		}
		case STATE_START_2:{
			next_if_timing_in_range(time_diff, 4000000, 4900000, STATE_BIT_LOW, dev_info);
			dev_info->value = 0;
			dev_info->counter = 0;
			break;
		}
		case STATE_BIT_HIGH:{
			if(time_diff < 827000){
				// bit zero
				dev_info->value <<= 1;
				dev_info->counter ++;
			}else if(time_diff < 1800000){ // was 1760000
				// bit one
				dev_info->value <<= 1;
				dev_info->value |= 0x1;
				dev_info->counter ++;
			}
			
			dev_info->decode_state = STATE_BIT_LOW;
			break;
		}
	}

	
}

static irqreturn_t gpio_irq_falling_handler(int irq,void *dev_id) 
{
	// get device the edge is working with
	struct ir_device_info *dev_info;
	dev_info = (struct ir_device_info *) dev_id;
	
	// current level on pin interpet the falling or rising edge
	bool current_pin_state;
	current_pin_state = gpio_get_value(dev_info->gpio_pin) > 0;


	// current time to calculate the delta between the edges
	uint64_t currentTime;
	currentTime = ktime_get_raw_fast_ns();
	// calculate time dif to last edge
	uint64_t time_diff;
	if(currentTime > dev_info->last_irq_time){
		time_diff = currentTime - dev_info->last_irq_time;
	}else{
		uint64_t zero_time = 0;
		time_diff = (zero_time - dev_info->last_irq_time) + currentTime;
	}

	// process depending on the edge
	if(current_pin_state && !dev_info->ir_last_state){
		ir_pin_rising(time_diff, dev_info);
		
		dev_info->last_irq_time = currentTime;
	}else if(!current_pin_state && dev_info->ir_last_state){
		ir_pin_falling(time_diff, dev_info);
		
		dev_info->last_irq_time = currentTime;
	}
	
	dev_info->ir_last_state = current_pin_state;

	return IRQ_HANDLED;
}


// **************** device managing functions **********************

static int ir_device_probe(struct platform_device *pdev)
{
	printk("IR Driver: ir_device_probe\n");
	printk("IR Driver: id.name = %s", pdev->name);

	// get configuration values of the device
	char const *device_name;
	uint32_t gpio_pin;

	if(of_property_read_string(pdev->dev.of_node, "device-name", &device_name)){
		printk("IR Driver: Unable to read name");
		pr_err("IR Driver: Unable to read name");
		goto r_end;
	}

	if(of_property_read_u32(pdev->dev.of_node, "gpio-pin", &gpio_pin)){
		printk("IR Driver: GPIO not defined");
		pr_err("IR Driver: GPIO not defined");
		goto r_end;
	}

	// allocate the device structure
	struct ir_device_info *dev_info;
	dev_info =  (struct ir_device_info*)devm_kzalloc(&pdev->dev, sizeof(struct ir_device_info), GFP_KERNEL);

	if(dev_info == NULL){
		printk("IR Driver: unable to alloc memory");
		pr_err("IR Driver: unable to alloc memory");
		goto r_end;
	}

	dev_info->gpio_pin = gpio_pin;
	dev_info->decode_state = 0;
	dev_info->pin_irq_number_rising = 0; 
	dev_info->last_irq_time = 0;
	dev_info->value = 0;
	dev_info->counter = 0;
	dev_info->ir_last_state = false;
	dev_info->last_value = 0;
	dev_info->ir_proc = NULL;


	// check if gpio pin is available
	if(gpio_is_valid(gpio_pin) == false){
		printk("IR Driver: GPIO %d is not valid\n", gpio_pin);
		pr_err("IR Driver: GPIO %d is not valid\n", gpio_pin);
		goto r_end;
	}

	// Requesting the GPIO
	if(gpio_request(gpio_pin, device_name) < 0){
		printk("IR Driver: GPIO %d request\n", gpio_pin);
		pr_err("IR Driver: GPIO %d request\n", gpio_pin);
		goto r_end;
	}
	printk("IR Driver: Successfully mapped in GPIO memory\n");
	// configure GPIO pin
	gpio_direction_input(gpio_pin);

	// setup the interrupt
	dev_info->pin_irq_number_rising = gpio_to_irq(gpio_pin);
	if (request_irq(dev_info->pin_irq_number_rising,             //IRQ number
					(void *)gpio_irq_falling_handler,   //IRQ handler
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,        //Handler will be called in raising edge
					"etx_device",               //used to identify the device name using this IRQ
					dev_info)) {                    //device id for shared IRQ
		pr_err("IR Driver: cannot register IRQ ");
		printk("IR Driver: cannot register IRQ ");
		goto r_gpio;
	}

	// create proc file to communicate from userspace
	dev_info->ir_proc = proc_create_data(device_name, 0666, NULL, &ir_proc_fops, dev_info);
	if(dev_info->ir_proc == NULL){
		printk("IR Driver: Unable to get file");
		pr_err("IR Driver: Unable to get file");
		goto r_irq_rising;
	}

	// set the device structure of the driver
    platform_set_drvdata(pdev, dev_info);
	
	printk("IR Driver: IR driver loaded!\n");

	return 0;

	r_proc_file:
		proc_remove(dev_info->ir_proc);
	r_irq_rising:
		free_irq(dev_info->pin_irq_number_rising,NULL);
	r_gpio:
		gpio_free(dev_info->gpio_pin);
	r_end:
		return -1;
}

static int ir_device_remove(struct platform_device *pdev)
{
	printk("mydevice_i2c_remove\n");
	struct ir_device_info *dev_info;
	dev_info = platform_get_drvdata(pdev);

	proc_remove(dev_info->ir_proc);
	free_irq(dev_info->pin_irq_number_rising,NULL);
    gpio_free(dev_info->gpio_pin);

	return 0;
}


// ********** setup driver ***********
static struct platform_driver mydevice_driver = {
	.driver = {
		.name			= DRIVER_NAME,
		.owner			= THIS_MODULE,
		.of_match_table = irdevice_of_match_table,
	},
	.probe			= ir_device_probe,
	.remove			= ir_device_remove,	
};

module_platform_driver(mydevice_driver);