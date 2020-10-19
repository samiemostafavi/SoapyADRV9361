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

0. Setup a static ip on eth0 interface of ADRV by appending the lines below to "rootfs/etc/network/interfaces"
   ```
   auto eth0
   iface eth0 inet static
   	address 10.0.9.1/24
   ```

1. Copy "board/iio-udp" folder into the SD card "rootfs/home/analog/iio-udp"

2. Connect to the board using Ethernet by setting a manual IP (board static ip: 10.0.9.1)

   ```
   ssh analog@10.0.9.1
   password: analog
   ```

3. Build the driver

   ```
   cd ~/iio-udp/; make
   ```

4. Insert the line below into "sudo crontab -e" (it is important to give root access to the server):

   ```
   @reboot sleep 10 && /home/analog/iio-udp/install.bash && /home/analog/iio-udp/Server > /home/analog/iio-udp/server_log.txt 2>&1
   ```

5. Reboot the board

All the logs are stored in "analog/iio-udp/server_log.txt".



## Dependencies

- [SoapySDR](https://github.com/pothosware/SoapySDR)

## Licensing Information

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

