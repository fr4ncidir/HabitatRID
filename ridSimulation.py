#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  ridSimulation.py
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

from platform import linux_distribution
from random import uniform
from time import sleep
from Producer import *
import sys,argparse,logging,json,subprocess

logging.basicConfig(format="%(filename)s\t%(levelname)s:\t%(message)s",level=logging.INFO)

RASPBERRY_INPUT_PIN = 17

SIMULATION_UPDATE =  \
"""PREFIX rdf:<http://www.w3.org/1999/02/22-rdf-syntax-ns#>
PREFIX rdfs:<http://www.w3.org/2000/01/rdf-schema#>
PREFIX hbt:<http://www.unibo.it/Habitat#>
DELETE {?pos 		hbt:hasCoordinateX 	?oldX. ?pos hbt:hasCoordinateY ?oldY} 
INSERT {	?id 		rdf:type 			hbt:ID.
			?id			hbt:hasLocation		hbt:Unknown.
			hbt:Unknown	rdf:type			hbt:Location.
			?id 		hbt:hasPosition 	?pos.
			?pos 		rdf:type 			hbt:Position. 
			?id 		rdfs:label 			?label. 
			?id 		hbt:role 			?role.
			?pos 		hbt:hasCoordinateX 	?x. 
			?pos 		hbt:hasCoordinateY 	?y} 
WHERE {{} UNION {OPTIONAL{?pos hbt:hasCoordinateX ?oldX. ?pos hbt:hasCoordinateY ?oldY}}}"""
		
def wait_next_iteration(iteration_type,timing):
	if iteration_type=="timer":
		sleep(timing)
	else:
		import RPi.GPIO as gpio
		first_run = True
		prev_input = 0
		try:
			while True:
				current_input = gpio.input(RASPBERRY_INPUT_PIN)
				if ((not prev_input) and current_input and (not first_run)):
					print "Button Pressed"
					break
				else:
					first_run = False
				prev_input = current_input
				sleep(0.05)
		except KeyboardInterrupt:
			logging.info("Caught Ctrl-C. Stop simulation...")
			return True
	return False

def json_config_open(json_config_file):
	json_format_array = ["sepa_ip","sepa_update_port","simulation","type","timing","iterations","x_topleft","y_topleft","ridUid","locations","role","label"]
	try:
		with open(json_config_file) as config_file:    
			config_data = json.load(config_file)
		for index in json_format_array:
			if index not in config_data:
				raise ValueError("Invalid configuration json: missing '{}' field".format(index))
	except Exception as openingjsonException:
		print openingjsonException
		logging.error("Caught exception in config file. Aborting.")
		return None,3
	return config_data,0

def simulate_new_position(kp,uid,pos,x,y,label,role):
	bounded_update = SIMULATION_UPDATE \
	.replace("?id","hbt:{}".format(uid)) \
	.replace("?pos","hbt:{}".format(pos)) \
	.replace("?x","\"{}\"".format(x)) \
	.replace("?y","\"{}\"".format(y)) \
	.replace("?role","\"{}\"".format(role)) \
	.replace("?label","\"{}\"".format(label))
	try:
		kp.produce(bounded_update)
	except Exception as e:
		print e
	
def main(args):
	print "ridSimulation.py - Francesco Antoniazzi <francesco.antoniazzi@unibo.it>"
	os_name,vers,iden = linux_distribution()
	if args.button==True:
		p1 = subprocess.Popen(["cat","/etc/issue"], stdout=subprocess.PIPE)
		p2 = subprocess.Popen(["grep","-c","Raspbian"], stdin=p1.stdout, stdout=subprocess.PIPE)
		p1.stdout.close()
		output = p2.communicate()[0]
		if output=="0\n":
			print "Button parameter is valid only if running from Raspberry! Detected {} OS".format(os_name)
			return 1
		else:
			print "Raspbian OS\nWelcome to the python RIDsimulator on Raspbian!\n"
			import RPi.GPIO as gpio
			gpio.setmode(gpio.BCM)
			gpio.setup(RASPBERRY_INPUT_PIN,gpio.IN)
	else:
		print "{} OS\nWelcome to the python RIDsimulator on {}!\n".format(os_name,os_name)
	config_data,error_code = json_config_open(args.JSON_configuration_file)
	if error_code:
		return error_code
		
	logging.info("SEPA ip: {}".format(config_data["sepa_ip"]))
	logging.info("SEPA update port: {}".format(config_data["sepa_update_port"]))
	sibHost = "http://{}:{}/sparql".format(config_data["sepa_ip"],config_data["sepa_update_port"])
	kp = Producer(sibHost)
	
	identifier = config_data["ridUid"]
	position = config_data["positionId"]
	label = config_data["label"]
	role = config_data["role"]
	logging.info("Identifier: {}".format(identifier))
	logging.info("Position: {}".format(position))
	logging.info("Label and Role: {}, {}".format(label,role))
	
	logging.info("Simulation max iterations: {}".format(config_data["iterations"]))
	logging.info("Simulation type: {}".format(config_data["type"]))
	
	sleep_time = int(config_data["timing"])/1000
	simulation_type = config_data["type"]
	if simulation_type=="timer":
		logging.info("Timing: {} s".format(sleep_time))
	else:
		logging.warning("Button interrupts for simulation... Check configurations on <github>")

	if config_data["simulation"]=="file":
		for i in range(min(int(config_data["iterations"]),len(config_data["locations"]))):
			# waits next iteration
			if wait_next_iteration(simulation_type,sleep_time):
				break
			new_x = config_data["locations"][i]["x"]
			new_y = config_data["locations"][i]["y"]
			logging.info("{}. ({},{})".format(i,new_x,new_y))
			# update sib
			simulate_new_position(kp,identifier,position,new_x,new_y,label,role)
	elif config_data["simulation"]=="random":
		logging.info("x_max={}".format(config_data["x_topleft"]))
		logging.info("y_max={}".format(config_data["y_topleft"]))
		new_x = uniform(0,float(config_data["x_topleft"]))
		new_y = uniform(0,float(config_data["y_topleft"]))
		for i in range(int(config_data["iterations"])):
			# waits next iteration
			if wait_next_iteration(simulation_type,sleep_time):
				break
			new_x = int(round(max(0,min(config_data["x_topleft"],new_x+uniform(-20,20)))))
			new_y = int(round(max(0,min(config_data["y_topleft"],new_y+uniform(-20,20)))))
			logging.info("{}. ({},{})".format(i,new_x,new_y))
			# update sib
			simulate_new_position(kp,identifier,position,new_x,new_y,label,role)
	else:
		logging.error("Config file error (unknown value simulation field)")
		return 5
	
	print "Simulation ended"
	return 0

if __name__ == '__main__':
	parser = argparse.ArgumentParser()
	parser.add_argument("JSON_configuration_file",help="Path to the json config file for simulation")
	parser.add_argument("-b","--button",help="If the script is running on Raspberry, this argument enables the hardware button interrupts.",action="store_true")
	arguments = parser.parse_args()
	sys.exit(main(arguments))
