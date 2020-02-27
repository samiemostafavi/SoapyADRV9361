# libiio - Library for interfacing industrial I/O (IIO) devices
#
# Copyright (C) 2014 Analog Devices, Inc.
# Author: Paul Cercueil <paul.cercueil@analog.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

CFLAGS = -Wall

.PHONY: all clean

all: server clean run

controller: Controller.o
	g++ -c Controller.cpp

iiodevice: IIODevice.o
	g++ -c IIODevice.cpp

udpserver: UDPServer.o
	g++ -c UDPServer.cpp

main: main.o
	g++ -c main.cpp
	
server: main.o UDPServer.o IIODevice.o Controller.o
	g++ -o Server Controller.o IIODevice.o UDPServer.o main.o -liio -lpthread

run:
	./Server

clean:
	rm -f *.o
