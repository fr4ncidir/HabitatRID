#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  ridSimulator_TCP.py
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
# This python script starts a server simulating a RID reader. After a setup request
# the script starts sending fake location data, until the requestor is down.
# 
# setup string format:
# [TIME_INTERVAL] [X_MIN] [X_MAX] [Y_MIN] [Y_MAX] [NUMBER_OF_ITERATIONS]

import socket,sys
from random import uniform
from time import sleep

def printUsage():
	print "USAGE:\n python ridSimulator_TCP.py ip_address ip_port [external_button]"

def main(args):
	if len(args)>3:
		printUsage()
		return -1
	elif len(args)==3:
		
	print "Welcome to the RIDSimulator!"
	
	# Create a TCP/IP socket
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	
	# Bind the socket to the port
	server_address = (args[1], int(args[2]))
	print "Starting server up on {}".format(server_address)
	sock.bind(server_address)
	
	# Listen for incoming connections
	sock.listen(1)

	# Wait for a connection
	print "Waiting for a connection..."
	connection, client_address = sock.accept()
	
	try:
		print "Got Connection from {}".format(client_address)
		# Receive the data in small chunks and retransmit it
		setup_string = ""
		while True:
			data = str(connection.recv(100)).replace("  "," ")
			print data
			setup_string = setup_string + data	
			setup_completeness = setup_string.count(" ")
			
			if setup_string=="STOP_RID_SIMULATION":
				break

			if setup_completeness==5:
				print "Received setup string: {}".format(setup_string)
				values = setup_string.split(" ")
				time_interval = int(values[0])
				x_min = float(values[1])
				x_max = float(values[2])
				y_min = float(values[3])
				y_max = float(values[4])
				n_iter = int(values[5])
				
				for iteration in range(n_iter):
					x = uniform(x_min,x_max)
					y = uniform(y_min,y_max)
					locationMessage = "Location: (x,y)=({},{})\n".format(x,y)
					connection.sendall(locationMessage)
					print "#{}) {}".format(iteration,locationMessage)
					sleep(time_interval)
				setup_string = ""
	finally:
		# Clean up the connection
		connection.close()
	print "RID simulator stopped"
	return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
