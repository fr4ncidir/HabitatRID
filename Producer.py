#!/usr/bin/python3

# inner requirements
from KP import *

# global requiremets
import requests


# class Producer
class Producer(KP):

    """The Producer class - Inherits from KP"""

    # constructor
    def __init__(self, host):

        """Constructor for the Producer class"""

        # superclass constructor
        KP.__init__(self, host, None)
        

    # update
    def produce(self, update):

        """This method is used to update the KB"""
        
        # update
        r = requests.post(self.httphost, data=update, headers={"Content-type":"application/sparql-update"})

        # return value
        if r.status_code == 200:
            return True
        else:
            return False
