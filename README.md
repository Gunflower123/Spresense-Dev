# Commands
1. Change into sdk dir..
- Check cable connection and flash recent code onto device
```
dmesg | grep "cp21.*attached"
make
tools/flash.sh -c /dev/ttyUSB0 -b 500000 nuttx.spk
```

- Make dir external to sdk for code dev
```
tools/mkappsdir.py myapps "My Apps"
tools/mkcmd.py -d myapps myfirstapp "My first app example"
```

# Links
https://github.com/sonydevworld/spresense-nuttx/blob/new-master/boards/arm/cxd56xx/spresense/include/cxd56_power.h  
https://github.com/sonydevworld/spresense-nuttx/blob/new-master/boards/arm/cxd56xx/spresense/include/cxd56_gpioif.h  

