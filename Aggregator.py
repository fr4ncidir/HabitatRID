#!/usr/bin/python

# local requirements
from Consumer import *
from Producer import *


# class Aggregator
class Aggregator(Producer, Consumer):

    """The Aggregator class - Inherits from Producer and Consumer"""

    # constructor
    def __init__(self, httphost, wshost, query, update):

        """Constructor for the Aggregator class"""

        # superclass constructor
        Producer.__init__(self, httphost, None)
        Consumer.__init__(self, httphost, wshost)

        # class attribute
        self.query = query
        self.update = update


    # aggregate
    def aggregate(self):

        """This method subscribes and performs updates when needed"""

        # TODO
        pass
