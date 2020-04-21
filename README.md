# Soapy SDR module for Analog Devices ADRV9361 SDR

## Installation instructions

```
git clone git@github.com:samiemostafavi/SoapyADRV9361.git
cd SoapyADRV9361
mkdir build
cd build
cmake ..
make
sudo make install

sudo sysctl -w net.core.rmem_default=12582912;
sudo sysctl -w net.core.wmem_default=12582912;
sudo sysctl -w net.core.wmem_max=12582912;
sudo sysctl -w net.core.rmem_max=12582912;

sudo sysctl -w net.ipv4.tcp_window_scaling=1;
sudo sysctl -w net.ipv4.tcp_reordering=3;
sudo sysctl -w net.ipv4.tcp_low_latency=1;
sudo sysctl -w net.ipv4.tcp_sack=0;
sudo sysctl -w net.ipv4.tcp_timestamps=0;
sudo sysctl -w net.ipv4.tcp_fastopen=1;


```

## Dependencies

- [SoapySDR](https://github.com/pothosware/SoapySDR)

