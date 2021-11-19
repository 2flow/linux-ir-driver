# links
## driver

driver is in /drivers/misc/irreceiver

* https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf

Tutorials: 
* https://elinux.org/RPi_GPIO_Code_Samples
* https://embetronicx.com/tutorials/linux/device-drivers/gpio-driver-basic-using-raspberry-pi/
* https://sysprogs.com/VisualKernel/tutorials/raspberry/leddriver/
* --> Needed: https://www.youtube.com/watch?v=lWzFFusYg6g
* --> Works:  https://embetronicx.com/tutorials/linux/device-drivers/gpio-driver-basic-using-raspberry-pi/
* --> Interrupts: https://embetronicx.com/tutorials/linux/device-drivers/gpio-linux-device-driver-using-raspberry-pi/#Get_the_IRQ_number_for_the_GPIO


## Device Tree:
 * https://elinux.org/images/4/48/Experiences_With_Device_Tree_Support_Development_For_ARM-Based_SOC's.pdf
 * https://elinux.org/Device_Tree_Usage
* https://linuxtut.com/en/0d13142863d9ed064e41/
* https://github.com/saiyamd/skeleton-dt-binding/blob/master/skeleton.c

Kompile Kernel:
* https://www.raspberrypi.com/documentation/computers/linux_kernel.html
* https://www.stephenwagner.com/2020/03/17/how-to-compile-linux-kernel-raspberry-pi-4-raspbian/
* https://bootlin.com/blog/enabling-new-hardware-on-raspberry-pi-with-device-tree-overlays/
* https://imxdev.gitlab.io/tutorial/How_to_add_DT_support_for_a_driver/


## Build kernel
* https://www.raspberrypi.com/documentation/computers/linux_kernel.html


## passing parameters from device tree
* https://stackoverflow.com/questions/24242565/how-to-pass-platform-device-information-present-in-my-platform-data-through-devi

# Problems
## openssl error
Install: ```sudo apt-get install libssl-dev```

## add to device tree
IF a new folder is created the KConfig and Makefle need to be added to the parrent.

# install kernel
```
KERNEL=kernel7l
#make bcm2711_defconfig
sudo make -j4 zImage modules dtbs
sudo make modules_install
sudo cp arch/arm/boot/dts/*.dtb /boot/
sudo cp arch/arm/boot/dts/overlays/*.dtb* /boot/overlays/
sudo cp arch/arm/boot/zImage /boot/kernel-flow.img 
# kernel=kernel-flow.img in  /boot/config.txt
```