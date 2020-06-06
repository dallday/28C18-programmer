#!/usr/bin/python3
#
#
# Sends a Nas format file over the serial port to the Arduino EEProm programmer
#         Usage:- sendnas.py filename.nas [serialport]
#
# I assumes the the .nas  file is in the format 
# 4 byte address followed by 8 bytes represented as 2 ascii characters 
#  followed by a checksum byte represented as 2 ascii characters 
#    the check sum is the mod 256 of the sum of all bytes
#
#  e.g. 0008 DF 62 D8 18 FB C3 DE 03 D8
#
# The process uses the serial port at 9600 baud rate to talk to the arduino
# 
# To use it I needed to install pyserial 
#     - module encapsulating access for the serial port 
#  as using python3 it was python3-serial in the package manager
#
# Note:- since the rom only pays attention to the first 2k of address space 
#        it should work if the address is 0xD000 or something.
#        
#   you will need to fix the serial port being used.
#
# David Allday
# version 0.5 - May 2020
#

import os
import sys
import serial
import time

# sets serial port
# windows version
# SerialPort = '//./com4'
# linux version
SerialPort = '/dev/ttyUSB0'

FileName=""
SendNasVersion = "sendnas version 0.5"

RECSIZE = 16


"""
	waitokay
	waits until it receives the OK response from the arduino
	or the ERR response 
    it prints out any responses it does get back
	
"""
def waitokay():

    bad = 0
    while True:
        s = ser.readline()
        # print (s)
        data=""
        try:
              data = s.decode("ASCII")
        except Exception as eReason:
              print ("Serial Data:",s)
              sys.exit("Problem converting serial port data to ASCII: " + str(eReason))
        print ("<" + data, end='') # print data but no linefeed as one is in the data
        if len(data) > 1 :
            # print (data[0:2])
            if data[0:2] == "OK" :
                #print ("Found OK")
                break
            if data[0:2] == "ER" :
                # print ("Found ER")
                sys.exit("Error reported by Arduino :(")
                break
        else:
            bad = bad + 1
        if bad > 100:
            sys.exit("To much chatter on the serial port :(")

###########################################
# start of code
###########################################
print(SendNasVersion)
# we assume that we are sending a .nas file 
# and it is the first parameter 
# and if required the 2nd parameter is the serial port to use.
# print ('Number of arguments:', len(sys.argv), 'arguments.')

if len(sys.argv) > 1:
    FileName = sys.argv[1]
if len(sys.argv) > 2:
    SerialPort = sys.argv[2]

if len(sys.argv) < 2 or len(sys.argv) > 3:
    print ("Usage:- sendnas.py filename.nas [serialport]")
    print ("      e.g. on linux")
    print ("         ./sendnas.py filename.nas /dev/ttyUSB0")
    print ("      e.g. on windows")
    print ("         sendnas.py filename.nas '//./com4'")
    sys.exit(" ")

# check if anything supplied for FileName
if len(FileName) == 0:
    sys.exit ("please supply filename as paramter ")
# check file
if os.path.isfile(FileName) != True :
    sys.exit ("File " + FileName + " does not exist")


##################################################
# open serial port, which should reset the arduino 
# this will then report version and say ok
##################################################
try:
    ser = serial.Serial(SerialPort,9600,stopbits=2 )
except Exception as eReason:
    print ( "Problem opening Serial port " + SerialPort + " Error: " + str(eReason) )
    sys.exit('Serial port issue')

print("Serial port " + SerialPort + " opened, waiting for Arduino to respond")
waitokay()

##################################
#  process file
##################################
print ("Processing " , FileName)
try:
    if len(FileName) != 0:
        fileObj = open(FileName, 'rb')
        for fileLine in fileObj:	# read all the lines in the file 1 at a time
            # remove the line feeds etc 
            fileLinecut = fileLine.strip()
            # empty line - end now
            if len(fileLinecut) > 0 :
                # . is used by Nascom to end a NAS file
                if (b'.' == fileLinecut[0:1]):
                    break;
                # add the control data around the data
                # newline at end to get it to process command
                senddata = b'W' + fileLinecut + b'\r\n'
                printdata = fileLinecut.decode("ASCII")
                print (">W "+printdata)
                #sys.stdout.flush()	# flush output buffer
                #ser.flush()
                ser.write(senddata)	# send the line to the ardunio
                waitokay()			# wait for the OK response
                #sys.stdout.flush()

        fileObj.close()
        print ("Proccesing completed.")

except Exception as eReason:
    print ( "Problem in processing " + FileName + " Error: " + str(eReason) )
    sys.exit('Problem in send code')

# end of code
