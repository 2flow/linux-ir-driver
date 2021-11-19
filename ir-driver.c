#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/proc_fs.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <linux/gpio.h>     //GPIO
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/poll.h>

static uint gpio_pin = 0;

// ***** Decoding steps *******
#define STATE_TYPE uint8_t

//  define decoding steps
#define STATE_START_1 0
#define STATE_BIT 2
#define STATE_BIT_HIGH 3
#define STATE_IDLE 4

#define STATE_START_2 5
#define STATE_BIT_LOW 6

static int decodingState = STATE_IDLE;

// used interrupt
static unsigned int pin_irq_number_rising;

// used to meassure the timing of the edges
static uint64_t last_irq_time = 0;

// value is used to build up the recent received one
static uint32_t value = 0;
// count received bits
static uint16_t counter = 0;

static bool ir_last_state = false;
static uint32_t last_value = 0;
static bool value_available = false;

// procfile
static struct proc_dir_entry *ir_proc = NULL;
static DECLARE_WAIT_QUEUE_HEAD(ir_wait);

// ************** user functinos ****************

// readfunction from the user
ssize_t ir_read(struct file *file, char __user *user, size_t size, loff_t *off){
	copy_to_user(user, &last_value, sizeof(last_value));
	last_value = 0;
	value_available = false;

	return sizeof(last_value);
}

// poll function from the user
 static unsigned int ir_poll(struct file *file, poll_table *wait)
 {
     poll_wait(file, &ir_wait, wait);
     if (value_available)
        return POLLIN | POLLRDNORM;

     return 0;
 }

static const struct proc_ops ir_proc_fops = {
	.proc_read = ir_read,
	.proc_poll = ir_poll
};

// ************* helper functions ******************

/**
 * Checks if the timing is correct and s
 */
inline static void next_if_timing_in_range(uint64_t time_diff,
	uint64_t lower_limit, 
	uint64_t uper_limit, 
	STATE_TYPE nextState){
	if((time_diff > lower_limit) && (time_diff < uper_limit)) {
		decodingState = nextState;
	}else{
		decodingState = STATE_IDLE;
	}
}


// ************* gpio interrupt functions ******************
static void ir_pin_rising(uint64_t currentTime){
	if(currentTime > last_irq_time){
		uint64_t time_diff = currentTime - last_irq_time;

		switch(decodingState){
			case STATE_START_1:{
				next_if_timing_in_range(time_diff, 8000000, 10000000, STATE_START_2);
				break;
			}
			case STATE_BIT_LOW:{
				if((time_diff < 400000) || (counter >= 31)){
					// all over set bit
					pr_info("Value: %d, Counter: %d\r\n", value,counter);
					last_value = value;
					decodingState = STATE_IDLE;
				}else if(time_diff < 827000){
					decodingState = STATE_BIT_HIGH;
				}

				break;
			}
		}

	}
}

static void ir_pin_falling(uint64_t currentTime){
	if(currentTime > last_irq_time){
		uint64_t time_diff = currentTime - last_irq_time;

		switch(decodingState){
			case STATE_IDLE:{
				decodingState = STATE_START_1;
				
				break;
			}
			case STATE_START_2:{
				next_if_timing_in_range(time_diff, 4000000, 4900000, STATE_BIT_LOW);
				value = 0;
	 			counter = 0;
				break;
			}
			case STATE_BIT_HIGH:{
				if(time_diff < 827000){
					// bit zero
					value <<= 1;
					counter ++;
				}else if(time_diff < 1760000){
					// bit one
					value <<= 1;
					value |= 0x1;
					counter ++;
				}
				
				decodingState = STATE_BIT_LOW;
				break;
			}
		}

	}
}


static irqreturn_t gpio_irq_falling_handler(int irq,void *dev_id) 
{
	uint64_t currentTime = ktime_get_raw_fast_ns();
	bool current_pin_state = gpio_get_value(gpio_pin) > 0;

	if(current_pin_state && !ir_last_state){
		ir_pin_rising(currentTime);
		
		last_irq_time = currentTime;
	}else if(!current_pin_state && ir_last_state){
		ir_pin_falling(currentTime);
		
		last_irq_time = currentTime;
	}

	ir_last_state = current_pin_state;
	
	return IRQ_HANDLED;
}

static int __init ir_driver_init(void)
{
	printk("Loading ir driver!\n");

	// check if gpio pin is available
	if(gpio_is_valid(gpio_pin) == false){
		pr_err("GPIO %d is not valid\n", gpio_pin);
		goto r_end;
	}
	
	// Requesting the GPIO
	if(gpio_request(gpio_pin,"GPIO_26") < 0){
		pr_err("ERROR: GPIO %d request\n", gpio_pin);
		goto r_end;
	}
	printk("Successfully mapped in GPIO memory\n");

	// configure GPIO pin
	gpio_direction_input(gpio_pin);

	// setup the interrupt
	pin_irq_number_rising = gpio_to_irq(gpio_pin);
	if (request_irq(pin_irq_number_rising,             //IRQ number
					(void *)gpio_irq_falling_handler,   //IRQ handler
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,        //Handler will be called in raising edge
					"etx_device",               //used to identify the device name using this IRQ
					NULL)) {                    //device id for shared IRQ
		pr_err("my_device: cannot register IRQ ");
		goto r_gpio;
	}
	
	// create proc file to communicate from userspace
	ir_proc = proc_create("ir-receiver", 0666, NULL, &ir_proc_fops);
	if(ir_proc == NULL){
		pr_err("IR Driver: Unable to get file");
		goto r_irq_rising;
	}

	printk("IR driver loaded!\n");
	return 0;

r_proc_file:
	proc_remove(ir_proc);
r_irq_rising:
	free_irq(pin_irq_number_rising,NULL);
r_gpio:
	gpio_free(gpio_pin);
r_end:
	return 0;

}

static void __exit ir_driver_exit(void)
{
	printk("Leaving ir driver!\n");
	proc_remove(ir_proc);
	free_irq(pin_irq_number_rising,NULL);
	gpio_free(gpio_pin);
	return;
}


// ************ module description ***************
module_init(ir_driver_init);
module_exit(ir_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("IR reading");
MODULE_DESCRIPTION("");
MODULE_VERSION("1.0");

// ********** parameter description ****************
module_param(gpio_pin, uint, 0);
MODULE_PARM_DESC(gpio_pin, "GPIO PIN used for the receiver");

// ************** device driver *****************

/*Matching table of devices handled by this device driver*/
/*Corresponds to the following in dts
	i2c@7e804000 {
		mydevice@18 {
			compatible = "mycompany,myoriginaldevice";
			reg = <0x18>;
		};
*/
static const struct of_device_id irdriver_of_match_table[] = {
	{.compatible = "flow,irdriver",},
	{ },
};
MODULE_DEVICE_TABLE(of, irdriver_of_match_table);

/*Register the table that identifies the device handled by this device driver*/
/*The important thing is the first name field. This determines the device name. The back is the data that can be used freely with this driver. Insert pointers and identification numbers*/
static struct i2c_device_id ir_reciver_idtable[] = {
	{"IrDriver", 0},
	{ }
};

MODULE_DEVICE_TABLE(i2c, ir_reciver_idtable);

static int irreceiver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk("mydevice_i2c_probe\n");
	if(id != NULL)
		printk("id.name = %s, id.driver_data = %d", id->name, id->driver_data);
	if(client != NULL)
		printk("slave address = 0x%02X\n", client->addr);

	/*Usually here to check if the device is supported by this driver*/

	int version;
	version = i2c_smbus_read_byte_data(client, 0x0f);
	printk("id = 0x%02X\n", version);

	return 0;
}

