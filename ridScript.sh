#! /bin/bash

# Author Francesco Antoniazzi
# francesco.antoniazzi@unibo.it

case $# in
	0)
		# shows help
		more README
		;;
	1)
		if [ $1 == "help" ]; then
			# shows help
			more README
		elif [ $1 == "setup" ]; then
			# install requirements
			echo System setup...
			sudo apt-get install libgsl0ldbl gsl-doc-info gsl-bin libgsl0-dev
		elif [ $1 == "run" ]; then
			# runs executable generated: check if executable exists and has permissions
			if [ -x "./RIDexecutable.exe" ]; then
				./RIDexecutable.exe
			else
				echo Error: RIDexecutable.exe not found or has not exe permissions
				exit 7
			fi
		else
			# error case
			echo Unexpected param $1...
			more README
			exit 3
		fi
		;;
	2)
		if [ $1 == "compile" ]; then
			# compile case
			if [ -a $2 ]; then
				# check if USB port exists and compile, give permissions
				echo Setting USB port name $2...
				sed -i "s|/dev/ttyUSB0|$2|g" main.c
				echo Compiling files...
				gcc -Wall -I/usr/local/include main.c RIDLib.c serial.c -o RIDexecutable.exe -lgsl -lgslcblas -lm
				chmod 777 RIDexecutable.exe
				sed -i "s|$2|/dev/ttyUSB0|g" main.c
			else
				# USB port does not exist
				echo Error: $2 doesn\'t exists
				exit 5
			fi
		elif [ $1 == "simulate" ]; then
			# simulation case
			if [ -a $2 ]; then
				# check if simulation input file exists
				echo RID simulation...
				cat /etc/issue | python ridSimulation.py $2 
			else
				# input format file does not exist
				echo Error: $2 doesn\'t exists
				exit 6
			fi
		else
			# error case
			echo Unexpected param $1...
			more README
			exit 4
		fi
		;;
	*)
		# default error case
		echo Too much parameters
		more README
		exit 2
		;;
esac
exit 0
