#!/usr/bin/python3

# local requirements
from KP import *

# global requiremets
import json
import _thread
import requests
import websocket


# class Consumer
class Consumer(KP):

    """The Consumer class - Inherits from KP"""

    # constructor
    def __init__(self, httphost, wshost, query):

        """Constructor for the Consumer class"""

        # superclass constructor
        KP.__init__(self, httphost, wshost)

        # class attributes
        self.query = query
        self.ws = None


    # query
    def consume(self):

        """This method is used to query the KB"""
        
        # update
        r = requests.post(self.httphost, data=self.query, headers={"Content-type":"application/sparql-query"})

        # return status and results
        if r.status == 200:
            return True, json.loads(r.text)
        else:
            return False, None
        

    # subscribe
    def subscribe(self, handler):

        """This method is used to subscribe to the KB"""

        # create a websocket
        websocket.enableTrace(True)
        self.ws = websocket.WebSocketApp(self.wshost,
                                         on_message = handler,
                                         on_error = self._on_error,
                                         on_close = self._on_close)
        
        # send the subscription
        self.ws.on_open = self.on_open
        self.ws.run_forever()
            

    # unsubscribe
    def unsubscribe(self, sub_id):

        """This method is used to unsubscribe from the KB"""
        
        # sending unsubscribe and closing subscription
        self.ws.send("unsubscribe=%s" % sub_id)
        self.ws.close()
        self.ws = None


    # websocket on_close
    def _on_close(self, ws):

        """Hidden method executed once the subscription is closed"""

        # setting websocket to None
        self.ws = None


    # websocket on_error
    def _on_error(self, ws, msg):

        """Hidden method used to handle errors from the websocket"""

        # close the websocket
        self.ws.close()


    # websocket on_open
    def on_open(self, ws):
    
        """Hidden method used by the subscribe function to
        send a query on the websocket"""

        # starting a new thread with the subscription
        def run(*args):
            ws.send("subscribe=%s" % self.query)
        _thread.start_new_thread(run, ())
