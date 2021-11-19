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
