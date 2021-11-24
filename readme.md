# IR Driver
This is a simple infrared driver for raspberry pi.

It can be added to the device tree and uses a given GPIO pin to read infrared remotes.

There are two different methods one as module to be loaded with insmod. 
And the second one is a actual device driver which needs to be added to the kernel.

##  Device tree
The following configuration uses GPIO 26 and exports the latest pressed button to **proc/${device-name}**. 
A read from this file will return 0 if nothing was pressed or a value of the button as UInt32.

```
irdriver {
    compatible = "flow,irdevice";
    status = "okay";
    device-name = "ir_device_1";
    gpio-pin = <26>;
};
```

## Documentation
The doc folder contains all the links and infos I used during the creation of the files.

# build and install kernel
```
KERNEL=kernel7l
#make bcm2711_defconfig
sudo make -j4 zImage modules dtbs
sudo make modules_install
sudo cp arch/arm/boot/dts/*.dtb /boot/
sudo cp arch/arm/boot/dts/overlays/*.dtb* /boot/overlays/
# use a different name of the kernel so revert to original is easy
sudo cp arch/arm/boot/zImage /boot/kernel-flow.img 

# to enable the kernel add the following line to the end of /boot/config.txt
# kernel=kernel-flow.img
```