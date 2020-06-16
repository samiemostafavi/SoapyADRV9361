# SoapySDR driver for Analog Devices ADRV9361 SDR

SoapyADRV9361 is a UDP based driver for Analog Devices [ADRV9361](https://www.analog.com/en/design-center/evaluation-hardware-and-software/evaluation-boards-kits/adrv9361-z7035.html#eb-overview) SDR which is faster and lighter than [LibIIO](https://wiki.analog.com/resources/tools-software/linux-software/libiio) network context. Compared to LibIIO, SoapyADRV9361 can use small buffers which is necessary for lateny-critical applications such as software LTE. Moreover, there is no TCP handshake overhead since SoapyADRV is based on UDP. UDP streaming is the best option when the link between the SDR board and the processing unit is Ethernet which is reliable.

<img src="doc/TestBedDetail.jpg?raw=true" alt="alt text" style="zoom: 67%;" />

## Installation instructions

### On the Processing Host

Make sure that [SoapySDR](https://github.com/pothosware/SoapySDR) is installed.

```
git clone git@github.com:samiemostafavi/SoapyADRV9361.git
cd SoapyADRV9361
mkdir build
cd build
cmake ..
make
sudo make install
```

Run these commands in the case of large packet drops or high latency:

```
sudo sysctl -w net.core.rmem_default=12582912;
sudo sysctl -w net.core.wmem_default=12582912;
sudo sysctl -w net.core.wmem_max=12582912;
sudo sysctl -w net.core.rmem_max=12582912;
```



### On the ADRV Board

1. Copy "board/iio-udp" folder into the SD card "rootfs/home/analog/iio-udp"

2. Insert the line below into "crontab -e":
   `@reboot make -C ~/iio-udp/make_log.tx 2>&1`
3. Reboot the board

All the logs are stored in "analog/iio-udp/server_log.txt".



## Dependencies

- [SoapySDR](https://github.com/pothosware/SoapySDR)

## Licensing Information

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

