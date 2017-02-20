#! /bin/bash

# Author Francesco Antoniazzi
# francesco.antoniazzi@unibo.it

if [ $# -ge 2 ]; then
# Usage description
	printf "USAGE: $0 setup \\n$0 USB_RID_address\\n"
	exit -1
fi
if [ "$1" == "setup" ]; then 
# install of gsl libraries
	sudo apt-get install libgsl0ldbl gsl-doc-info gsl-bin libgsl0-dev
	printf "\\nAnd now you can rerun without parameters\\n"
else
	echo "checking if USB $1 exists..."
	if [ -a $1 ]; then
		echo Setting USB port name $1...
# puts in the .c file the correct path to RID usb
		sed -i "s|/dev/ttyUSB0|$1|g" main.c
		echo Compiling files...
# compiles into RIDexecutable.exe
		gcc -Wall -I/usr/local/include main.c RIDLib.c serial.c -o RIDexecutable.exe -lgsl -lgslcblas -lm
	else
		echo "$2 doesn't exist"
	fi
fi