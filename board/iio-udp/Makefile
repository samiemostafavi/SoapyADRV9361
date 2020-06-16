# Place the line below in "crontab -e" :
# @reboot make -C ~/iio-udp/make_log.tx 2>&1


CFLAGS = -Wall

.PHONY: all clean

all: server clean install run

Controller.o: Controller.cpp
	g++ -c Controller.cpp -O3 

IIODevice.o: IIODevice.cpp
	g++ -c IIODevice.cpp -O3

UDPServer.o: UDPServer.cpp
	g++ -c UDPServer.cpp -O3

main.o: main.cpp
	g++ -c main.cpp -O3
	
server: main.o UDPServer.o IIODevice.o Controller.o
	g++ Controller.o IIODevice.o UDPServer.o main.o -lad9361 -liio -lpthread -o Server

run:
	./Server > server_log.txt 2>&1

clean:
	rm -f *.o

install:
	./install.bash > install_log.txt 2>&1
