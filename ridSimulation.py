#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  ridSimulation.py
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

from platform import linux_distribution
from random import uniform
from time import sleep
import sys
import logging
import json

logging.basicConfig(format="ridSimulation.py\t%(levelname)s:\t%(message)s")

def printUsage():
	print "USAGE:\n" + \
		"python ridSimulation.py help\n" + \
		"\tprints this guide\n" + \
		"python ridSimulation.py <configuration file>\n" + \
		"\tstarts simulation using the configuration file (non Raspberry)\n" + \
		"python ridSimulation.py <configuration file> Raspberry\n" + \
		"\tstarts simulation using the configuration file (Raspberry)\n"
		
def wait_next_iteration(iteration_type,timing):
	if iteration_type=="timer":
		sleep(sleep_time)
	else:
                import RPi.GPIO as gpio
		logging.warning("Button interrupts for simulation... Check configurations on <github>")
		prev_input = 0
		try:
			while True:
				current_input = gpio.input(17)
				if ((not prev_input) and current_input):
					print("Button Pressed")
				prev_input = current_input
				sleep(0.05)
		except KeyboardInterrupt:
			logging.info("Caught Ctrl-C. Stop simulation...")
			return True
	return False

		
def os_noRaspbian_message():
	os_name,vers,iden = linux_distribution()
	notes = "ridSimulation.py\tTrigger simulation by button is not possible here."
	print "ridSimulation.py\t{}\n\nWelcome to the RIDsimulator on {}!\n{}".format(os_name,os_name,notes)
	return True

def json_config_open(json_config_file):
	json_format_array = ["sib_ip","sib_port","simulation","type","timing","iterations","x_topleft","y_topleft","locations"]
	try:
		with open(json_config_file) as config_file:    
			config_data = json.load(config_file)
		for index in json_format_array:
			if index not in config_data:
				raise ValueError("Invalid configuration json: missing '{}' field".format(index))
	except:
		logging.error("Caught exception in config file. Aborting.")
		return None,3
	return config_data,0
	
def main(args):
	print "\nridSimulation.py - Francesco Antoniazzi <francesco.antoniazzi@unibo.it>"
	ignore_button_type = False
	
	if len(args)==1:
		printUsage()
		return 1
	elif len(args)==2:
		if args[1]=="help":
			printUsage()
			return 2
		else:
			ignore_button_type = os_noRaspbian_message()
			config_data,error_code = json_config_open(args[1])
			if error_code:
				return error_code
	elif len(args)==3:
		if args[2]!="Raspberry":
			ignore_button_type = os_noRaspbian_message()
		else:
			print "ridSimulation.py\tRaspbian\n\nWelcome to the RIDsimulator on Raspbian!\n"
			import RPi.GPIO as gpio
			gpio.setmode(gpio.BCM)
			gpio.setup(17,gpio.IN)
		config_data,error_code = json_config_open(args[1])
		if error_code:
			return error_code
	else:
		logging.error("Unexpected arguments. Aborting.")
		printUsage()
		return 4
	
	logging.info("SIB ip: {}".format(config_data["sib_ip"]))
	logging.info("SIB port: {}".format(config_data["sib_port"]))
	logging.info("Simulation max iterations: {}".format(config_data["iterations"]))
	
	if ignore_button_type:
		ignored = "IGNORED"
	else:
		ignored = ""
	logging.info("Simulation type: {} {}".format(config_data["type"],ignored))
	
	sleep_time = None
	simulation_type = "button"
	if ((config_data["type"]=="timer") or ignore_button_type):
		sleep_time = int(config_data["timing"])/1000
		logging.info("Timing: {} s".format(sleep_time))
		simulation_type = "timer"
	
	if config_data["simulation"]=="file":
		for i in range(min(int(config_data["iterations"]),len(config_data["locations"]))):
			new_x = config_data["locations"][i]["x"]
			new_y = config_data["locations"][i]["y"]
			logging.info("{}. ({},{})".format(i,new_x,new_y))
			# update sib
			
			# waits next iteration
			if wait_next_iteration(simulation_type,sleep_time):
				break
	elif config_data["simulation"]=="random":
		logging.info("x_max={}".format(config_data["x_topleft"]))
		logging.info("y_max={}".format(config_data["y_topleft"]))
		for i in range(int(config_data["iterations"])):
			new_x = uniform(0,float(config_data["x_topleft"]))
			new_y = uniform(0,float(config_data["y_topleft"]))
			logging.info("{}. ({},{})".format(i,new_x,new_y))
			# update sib
			
			# waits next iteration
			if wait_next_iteration(simulation_type,sleep_time):
				break
	else:
		logging.error("Config file error (unknown value simulation field)")
		return 5
	logging.info("Simulation ended")
	return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
