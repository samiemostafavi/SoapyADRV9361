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
	./Server

clean:
	rm -f *.o

install:
	./install.bash
