#! /bin/sh

# Author Francesco Antoniazzi
# francesco.antoniazzi@unibo.it

case $# in
	0)
		cat README
		exit 0
		;;
	1)
		if [ $1 == "help" ]; then
			cat README
			exit 0
		elif [ $1 == "setup" ]; then
			sudo apt-get install libgsl0ldbl gsl-doc-info gsl-bin libgsl0-dev
		elif [ $1 == "run" ]; then
			./RIDexecutable.exe
		elif [ $1 == "simulate" ]; then
			
		else
			cat README
			exit 3
		fi
		;;
	2)
		if [ $1 == "compile" ]; then
			if [ -a $2 ]; then
				echo Setting USB port name $2...
				sed -i "s|/dev/ttyUSB0|$1|g" main.c
				echo Compiling files...
				gcc -Wall -I/usr/local/include main.c RIDLib.c serial.c -o RIDexecutable.exe -lgsl -lgslcblas -lm
				chmod 777 RIDexecutable.exe
			else
				echo Error: $2 doesn\'t exists
				exit 5
			fi
		else
			cat README
			exit 4
		fi
		;;
	*)
		echo Too much parameters
		cat README
		exit 2
		;;
esac
