# 28C18-programmer
A simple EEPROM programmer for 28C18 chips

 Thanks to K Adcock for the original code
 * http://danceswithferrets.org/geekblog/?page_id=903

I have adapted his code and circuits to program the 28C16 2kx8 EEPROM chips to use for my Nascom 2

The arduino code needs to be loaded onto a Mega 256.

You can interface to the Arduino using the Serial Monitor
   See the .ino file for details.
There are also 2 python scripts.

    sendnas.py - which will send a .nas file to the programmer.

    readnas.py - which will read the contents of the EEPROM and optionally place that in a file.

The scripts have been tested on Debian system but should work under windows as well.
