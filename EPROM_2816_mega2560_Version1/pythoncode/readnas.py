#!/usr/bin/python3
#
#
# Reads details from EEprom in a Nas format file over the serial port.
# 	optionally saves output to a new file.
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
import string

# sets serial port
# windows version
# SerialPort = '//./com4'
# linux version
SerialPort = '/dev/ttyUSB0'

ReadNasVersion = "readnas version 0.5"
StartAddress = ""
ReadBytes = ""
FileName=""
OutputToFile=False

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
            # we have data from ardunio - check if needs to be saved to file
            if  OutputToFile == True:
                fileObj.write(s)

        else:
            bad = bad + 1
        if bad > 100:
            sys.exit("To much chatter on the serial port :(")

###########################################
# start of code
###########################################
print(ReadNasVersion)
# check what has been supplied
# print ('Number of arguments:', len(sys.argv), 'arguments.')

if len(sys.argv) > 1:
    StartAddress = sys.argv[1]
if len(sys.argv) > 2:
    ReadBytes = sys.argv[2]
if len(sys.argv) > 3:
    SerialPort = sys.argv[3]
if len(sys.argv) > 4:
    FileName = sys.argv[4]


if len(sys.argv) < 2 or len(sys.argv) > 5:
    print ("Usage:- readnas.py startaddress [NumberOfBytes [serialport [filename.nas]]] ")
    print ("      e.g. on linux")
    print ("         ./sendnas.py D000 7FF /dev/ttyUSB0")
    print ("      e.g. on windows")
    print ("         sendnas.py D000 7FF '//./com4'")
    sys.exit(" ")

# check start address

# check if anything supplied for FileName
if len(FileName) != 0:
    # check file
    if os.path.isfile(FileName) == True :
        sys.exit ("File " + FileName + " exists - please choose a different name")

# check if hex digits supplied as start address
if all(c in string.hexdigits for c in StartAddress) != True:
    sys.exit ("Start Address " + StartAddress + " invalid hex value")
# check if hex digits supplied as number bytes
if all(c in string.hexdigits for c in ReadBytes) != True:
    sys.exit ("Number of bytes " + ReadBytes + " invalid hex value")


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
print ("Processing")
try:
    # if required open file
    if len(FileName)> 0:
        fileObj = open(FileName, 'wb')
        print ("File ",FileName," created")
        OutputToFile=True

    # send read command to ardunio
    sendstring =  "R" + StartAddress + " " + ReadBytes + "\r\n"
    senddata = sendstring.encode('utf-8')
    ser.write(senddata)	# send the line to the ardunio
    waitokay()			# wait for the OK response

    if len(FileName)> 0:
        OutputToFile=False
        fileObj.close()

    print ("Proccesing completed.")

except Exception as eReason:
    print ( "Problem in processing Error: " + str(eReason) )
    sys.exit('Problem in read code')

# end of code
