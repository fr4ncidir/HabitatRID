#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  ridSimulator_Check.py
#  
#  Copyright 2017 Francesco Work <francesco.antoniazzi@unibo.it>
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

import socket,sys

def printUsage():
	print "USAGE:\n python ridSimulator_Check.py ip_address_dest ip_port_dest"

def main(args):
	if len(args)!=3:
		printUsage()
		return -1
	print "Welcome to the ridSimulator check!"
	
	# Create a TCP/IP socket
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

	# Connect the socket to the port where the server is listening
	server_address = (args[1], int(args[2]))
	print "Pointing to ridSimulator server on {}".format(server_address)
	sock.connect(server_address)
	
	try:
		while True:
			# reads from keyboard and sends setup message
			message = raw_input("Please insert setup message (or exit): ")
			sock.sendall(message)
			if message=="exit":
				break
			values = message.split(" ")
			n_iter = int(values[5])
			print "Waiting for responses..."
			for iterations in range(n_iter):
				# Waits for the response
				data = sock.recv(100)
				print data
	finally:
		sock.close()
	print "ridSimulator check ended!"
	return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
