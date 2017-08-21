#!/usr/bin/python

import time
import smbus2 as smbus
import signal
import sys
import paho.mqtt.client as mqtt
import RPi.GPIO as GPIO
import itertools
import configparser
import os

schlossstatus = False
clubstatus = False
last_clubstatus = 0
stromstatus = False

ampel = 'unknown'

class Relay():
    global bus

    def __init__(self):
        self.DEVICE_ADDRESS = 0x20
        self.DEVICE_REG_MODE1 = 0x06
        self.DEVICE_REG_DATA = 0xff
        bus.write_byte_data(self.DEVICE_ADDRESS, self.DEVICE_REG_MODE1, self.DEVICE_REG_DATA)

    def strom(self, state): # relay 1
        if state:
            self.DEVICE_REG_DATA &=  ~(0x1 << 0)
            bus.write_byte_data(self.DEVICE_ADDRESS, self.DEVICE_REG_MODE1, self.DEVICE_REG_DATA)
        else:
            self.DEVICE_REG_DATA |= (0x1 << 0)
            bus.write_byte_data(self.DEVICE_ADDRESS, self.DEVICE_REG_MODE1, self.DEVICE_REG_DATA)

    def yellow(self, state): # relay 2
        if state:
            self.DEVICE_REG_DATA &=  ~(0x1 << 1)
            bus.write_byte_data(self.DEVICE_ADDRESS, self.DEVICE_REG_MODE1, self.DEVICE_REG_DATA)
        else:
            self.DEVICE_REG_DATA |= (0x1 << 1)
            bus.write_byte_data(self.DEVICE_ADDRESS, self.DEVICE_REG_MODE1, self.DEVICE_REG_DATA)

    def red(self, state): # relay 3
        if state:
            self.DEVICE_REG_DATA &=  ~(0x1 << 2)
            bus.write_byte_data(self.DEVICE_ADDRESS, self.DEVICE_REG_MODE1, self.DEVICE_REG_DATA)
        else:
            self.DEVICE_REG_DATA |= (0x1 << 2)
            bus.write_byte_data(self.DEVICE_ADDRESS, self.DEVICE_REG_MODE1, self.DEVICE_REG_DATA)

    def green(self, state): # relay 4
        if state:
            self.DEVICE_REG_DATA &=  ~(0x1 << 3)
            bus.write_byte_data(self.DEVICE_ADDRESS, self.DEVICE_REG_MODE1, self.DEVICE_REG_DATA)
        else:
            self.DEVICE_REG_DATA |= (0x1 << 3)
            bus.write_byte_data(self.DEVICE_ADDRESS, self.DEVICE_REG_MODE1, self.DEVICE_REG_DATA)



if __name__=="__main__":
    path = os.path.dirname(os.path.realpath(__file__))
    # read config
    config = configparser.ConfigParser()
    config['mqtt'] = {'host': 'localhost', 'port': 1883, 'auth': 'no'}
    config.read(path + '/configuration.ini')
    
    # init relay
    bus = smbus.SMBus(1)
    relay = Relay()

    # catch SIGINT
    def endProcess(signalnum = None, handler = None):
        relay.strom(True)

        relay.red(True)
        relay.yellow(False)
        relay.green(True)

        try:
            client.loop_stop()
        finally:
            sys.exit()
    
    signal.signal(signal.SIGINT, endProcess)

    # connect to MQTT
    client = mqtt.Client()
    if config.getboolean('mqtt', 'auth'):
        client.username_pw_set(config.get('mqtt', 'user'), config.get('mqtt', 'password'))

    client.connect(config.get('mqtt', 'host'), config.getint('mqtt', 'port'), 60)
    client.loop_start()
    
    # setup GPIO pins
    #
    # pin 7 (GPIO  4): clubstatus, high == off, low == on
    # pin 11 (GPIO 17: state of lock, high == locked, low == unlocked
    GPIO.setmode(GPIO.BOARD) 
    GPIO.setup(7, GPIO.IN)
    GPIO.setup(11, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    
    last_state = None

    # loop forever
    for i in itertools.count():
        time.sleep(1)
        
        # Clubstatus
        state = False if GPIO.input(7) else True

        if state != last_state or i % 10 == 0:
            client.publish("/public/eden/clubstatus", int(state))
        last_state = state
        
    	if state:
	        last_clubstatus = int(time.time())

        clubstatus = state
        
        # Schloss
        schlossstatus = True if GPIO.input(11) else False

        # Strom
        if last_clubstatus > (int(time.time())-20) and not stromstatus:
            relay.strom(True)
            stromstatus = True

        if last_clubstatus < (int(time.time())-20) and stromstatus:
    	    relay.strom(False)
            stromstatus = False

        # Ampel
        
        # Club ist offen, Schloss ist offen -> Gruen
        if clubstatus and not schlossstatus and ampel != 'green':
            relay.red(False)
            relay.yellow(False)
            relay.green(True)
            
            ampel = 'green'
        
        # Club ist zu, Schloss ist offen -> Gelb
        elif not clubstatus and not schlossstatus and ampel != 'yellow':
            relay.rede(False)
            relay.yellow(True)
            relay.green(False)
            
            ampel = 'yellow'
        
        # Club ist offen, Schloss ist zu -> Gelb-Rot
        elif clubstatus and schlossstatus and ampel != 'yellow-red':
            relay.red(True)
            relay.yellow(True)
            relay.green(False)
            
            ampel = 'yellow-red'

        # everything else -> Rot
        elif not clubstatus and schlossstatus and ampel != 'red':
            relay.red(True)
            relay.yellow(False)
            relay.green(False)
            
            ampel = 'red'

        # print log
        print(time.strftime("%Y-%m-%d %H:%M:%S") + " | Club: " + str(clubstatus) + " - Schloss: " + str(schlossstatus) + " - Strom: " + str(stromstatus) + " - Ampel: " + ampel)

# EOF
