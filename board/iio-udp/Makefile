# Place the line below in "crontab -e" :
# @reboot make -C ~/iio-udp/make_log.tx 2>&1


CFLAGS = -Wall

.PHONY: all clean

all: server clean

Controller.o: Controller.cpp
	g++ -c Controller.cpp -O0

IIODevice.o: IIODevice.cpp
	g++ -c IIODevice.cpp -O0

UDPServer.o: UDPServer.cpp
	g++ -c UDPServer.cpp -O0

main.o: main.cpp
	g++ -c main.cpp -O0
	
server: main.o UDPServer.o IIODevice.o Controller.o
	g++ Controller.o IIODevice.o UDPServer.o main.o -lad9361 -liio -lpthread -o Server

clean:
	rm -f *.o

