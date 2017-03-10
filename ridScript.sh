#! /bin/bash
# -*- coding: utf-8 -*-
#
#  ridScript.sh
#  
#  Copyright 2017 Francesco Antoniazzi <francesco.antoniazzi@unibo.it>
#  
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#  
#  

os_version=`cat /etc/issue | grep -c Raspbian`
if [ $os_version -ne 0 ]; then
	echo Auto git pull...
	if [ `git pull | grep -c Already` -eq 0 ]; then
		exit 0
	else
		echo Up-to-date
	fi
else
	echo "Maybe you need to git pull?"
fi

case $# in
	0)
		# shows help
		cat README | grep '\$'
		;;
	1)
		if [ $1 == "help" ]; then
			# shows help
			more README
		elif [ $1 == "setup" ]; then
			# install requirements
			echo System setup...
			sudo apt-get install libgsl0ldbl gsl-doc-info gsl-bin libgsl0-dev curl libcurl4-gnutls-dev python-pip
			if [ $os_version -ne 0 ]; then
				echo Raspbian os...
				sudo pip install RPi.GPIO
			fi
		elif [ $1 == "run" ]; then
			# runs executable generated: check if executable exists and has permissions
			# no parameter is given to RIDexecutable.exe
			if [ -x "./RIDexecutable.exe" ]; then
				./RIDexecutable.exe -c 1
			else
				echo Error: RIDexecutable.exe not found or has not exe permissions
				exit 7
			fi
		else
			# error case
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
				gcc -Wall -I/usr/local/include main.c RIDLib.c serial.c -o RIDexecutable.exe -lgsl -lgslcblas -lm -lcurl
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
				if [ $os_version -ne 0 ]; then
					python ridSimulation.py $2 Raspberry
				else
					python ridSimulation.py $2
				fi
			else
				# input format file does not exist
				echo Error: $2 doesn\'t exists
				exit 6
			fi
		elif [ $1 == "run" ]; then
			# runs executable generated: check if executable exists and has permissions
			# gives the second parameter to RIDexecutable
			if [ -x "./RIDexecutable.exe" ]; then
				./RIDexecutable.exe $2
			else
				echo Error: RIDexecutable.exe not found or has not exe permissions
				exit 7
			fi
		else
			# error case
			echo Unexpected param $1...
			more README
			exit 4
		fi
		;;
	3)
		if [ $1 == "run" ]; then
			# runs executable generated: check if executable exists and has permissions
			# gives the second parameter to RIDexecutable, as well as USB port name
			./RIDexecutable.exe $2 $3
		fi
	*)
		# default error case
		echo Too much parameters
		more README
		exit 2
		;;
esac
exit 0
