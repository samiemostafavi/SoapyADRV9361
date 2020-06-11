# SoapySDR driver for Analog Devices ADRV9361 SDR

SoapyADRV9361 is a UDP based driver which is faster and lighter than LibIIO network context. Compared to LibIIO, SoapyADRV can use small buffers which is necessary for lateny-critical applications such as software LTE. Moreover, there is no TCP timing and processing overhead since SoapuADRV is based on UDP. UDP is the best option when the link between the SDR board and the processing unit is Ethernet which is reliable.

![alt text](doc/TestBedDetail.jpg?raw=true)

## Installation instructions

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

```
sudo sysctl -w net.ipv4.tcp_window_scaling=1;
sudo sysctl -w net.ipv4.tcp_reordering=3;
sudo sysctl -w net.ipv4.tcp_low_latency=1;
sudo sysctl -w net.ipv4.tcp_sack=0;
sudo sysctl -w net.ipv4.tcp_timestamps=0;
sudo sysctl -w net.ipv4.tcp_fastopen=1;
```

## Dependencies

- [SoapySDR](https://github.com/pothosware/SoapySDR)

## Licensing Information

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

