#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  prova_rid.py
#  
#  Copyright 2018 Francesco Antoniazzi <francesco.antoniazzi@unibo.it>
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
import serial

def main(args):
	ser = serial.Serial("/dev/ttyUSB0",baudrate=115200,parity=serial.PARITY_NONE,stopbits=serial.STOPBITS_ONE,bytesize=serial.EIGHTBITS,timeout=None)
	ser.write(str.encode("+"))
	print("QUI1")
	ser.write(str.encode("<"))
	print("QUI2")
	while True:
		data = ser.read(2)
		print(data)
		break
	while True:
		data = ser.read(4)
		print(data)
		break
	for i in range(40):
		ser.write(str.encode("<"))
		while True:
			data = ser.read(3)
			print(data)
			break
	ser.write(str.encode(">"))
	while True:
		try:
			data = ser.read(1)
			print(data)
		except KeyboardInterrupt:
			print("Bye")
	return 0

if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv))
