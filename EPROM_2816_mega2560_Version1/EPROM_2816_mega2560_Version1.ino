/*
 * EEPROM Programmer - code for an Arduino mega 2560
 * updated by David Allday May 2020
 *  to handle the 28C16 24 pin style EEProm chips.
 *  
 *
 * Thanks to K Adcock for the original code
 * http://danceswithferrets.org/geekblog/?page_id=903
 *
 * This software uses a 9600-8N1 serial port.
 * 
 * Changed to use the NASCOM NAS file format to transfer data
 *    The checksum process used is the NAS file format checksum
 *    It adds the all the byte values (including the 2 bytes in the address) and does a mod 256 on the result.
 *    
 * The write routine checks using the DATA POLLING after a write to ensure the write process has completed.
 * 
 * Indivdual lines can be written using the Serial monitor but 
 * there is a Python program to run on the PC it sends multiple lines from a .NAS file 
 * and waits for the OK between each one.
 * 
 *
 * R                                      - read eeprom address 0 for 16 bytes 
 * R [hex address]                        - reads bytes of data from the EEPROM from addresses for 16 bytes
 * R [hex address] [hex value]            - reads bytes of data from the EEPROM from address for specified number of bytes
 *                                          Any data read from the EEPROM will have a checksum appended to it.
 *                                        
 * W [data]                               - writes data to EEPROM using the following format
 *       <four byte hex address> <data in hex, two characters per byte> <check sum in  hex>  
 *                          space between each set max 16, plus cksum at end
 *        e.g.
 *            W 07F0 42 07 C0 03 7D 03 78 00 FB
 *            W 07F8 79 02 8E 00 62 03 B5 05 27
 *                                        Note: since the EEPROM is has addresses 0000 to 07FF ( first 11 bits )
 *                                              sending address 0F78 or 17F8 is the same as 07F8
 *
 * P                                      - set write-protection bit (Atmels only, AFAIK) - not tested :)
 * U                                      - clear write-protection bit (ditto)
 * V                                      - prints the version string
 *
 * If a string of data is sent with an optional checksum, then this will be checked
 * before anything is written.
 *
 */

 
// the general opion seems to be that static const is better than #define in most cases
// and the IDE will optimise it during compile so it uses not more space in the generated code.

static const char hex[] =
{
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

const char version_string[] = "28C18 EEPROM Ver D2";


// these defines the pins used to handle Charlieplexing the LEDS.
// used to drive 3 LEDS but can do a 6
static const int  LEDPin1 = 23;
static const int  LEDPin2 = 22;
static const int  LEDPin3 = 24;

// the eeprom control pins
static const int eeprom_nWE     = 37;
static const int eeprom_nOE     = 39;
static const int eeprom_nCE     = 43;

// the eeprom data pins
static const int eepromData0   = 46;
static const int eepromData1   = 48;
static const int eepromData2   = 50;
static const int eepromData3   = 53;
static const int eepromData4   = 51;
static const int eepromData5   = 49;
static const int eepromData6   = 47;
static const int eepromData7   = 45;

// the eeprom address pins
static const int eepromAddress0   = 44;
static const int eepromAddress1   = 42;
static const int eepromAddress2   = 40;
static const int eepromAddress3   = 38;
static const int eepromAddress4   = 36;
static const int eepromAddress5   = 34;
static const int eepromAddress6   = 32;
static const int eepromAddress7   = 30;
static const int eepromAddress8   = 33;
static const int eepromAddress9   = 35;
static const int eepromAddress10  = 41;

byte g_cmd[200]; // strings received from the controller will go in here - up it  to 200 just in case DA
static const int kMaxBufferSize = 16;
byte storageBuffer[kMaxBufferSize+1]; // allow for 0 on a full buffer and csum if getting write data

static const int kOutputSize = 8;  // number we will output on each line

static const long int maxAddress=2047;   // 2048 bytes 0 to 2047

static const long int k_uTime_WritePulse_uS = 1; 
static const long int k_uTime_ReadPulse_uS = 1;
// (to be honest, both of the above are about ten times too big - but the Arduino won't reliably
// delay down at the nanosecond level, so this is the best we can do.)

// the setup function runs once when you press reset or power the board
void setup()
{
  Serial.begin(9600);

  // address lines are ALWAYS outputs
  pinMode(eepromAddress0,  OUTPUT);
  pinMode(eepromAddress1,  OUTPUT);
  pinMode(eepromAddress2,  OUTPUT);
  pinMode(eepromAddress3,  OUTPUT);
  pinMode(eepromAddress4,  OUTPUT);
  pinMode(eepromAddress5,  OUTPUT);
  pinMode(eepromAddress6,  OUTPUT);
  pinMode(eepromAddress7,  OUTPUT);
  pinMode(eepromAddress8,  OUTPUT);
  pinMode(eepromAddress9,  OUTPUT);
  pinMode(eepromAddress10, OUTPUT);
  
  // control lines are ALWAYS outputs
  // set high before switching to output
  digitalWrite(eeprom_nCE, HIGH); 
  digitalWrite(eeprom_nOE, HIGH);
  digitalWrite(eeprom_nWE, HIGH); // not writing
  pinMode(eeprom_nCE, OUTPUT);
  pinMode(eeprom_nOE, OUTPUT);
  pinMode(eeprom_nWE, OUTPUT);

  SetDataLinesAsInputs();
  SetAddress(0);

  // the 3 pins controlling LEDS
  // makes sure all LED outputs are low before turning them on
  digitalWrite(LEDPin1,LOW);
  digitalWrite(LEDPin2,LOW);
  digitalWrite(LEDPin3,LOW);
  LedsOff();  // ensure al leds are off

  Serial.println(version_string);
  Serial.println("OK");

}

void loop()
{

  
  while (true)
  {
    // set controls to safe mode
    digitalWrite(eeprom_nCE, HIGH); 
    digitalWrite(eeprom_nOE, HIGH);
    digitalWrite(eeprom_nWE, HIGH); // not writing
    SetDataLinesAsInputs();
    
    SetGreenLED();
    ReadString();
    LedsOff();

/*
  int currentPosition = 0;
  Serial.print("received'");
  while ( g_cmd[currentPosition] != 0)
  {
    Serial.print(char(g_cmd[currentPosition]));
    currentPosition+=1;
  }
  Serial.println("'");
  */  
    switch (g_cmd[0])
    {
      case 'v':
      case 'V':Serial.println(version_string);Serial.println("OK"); break;
      case 'p':
      case 'P': SetSDPState(true); break;
      case 'u':
      case 'U': SetSDPState(false); break;
      case 'r':
      case 'R': ReadEEPROM(); break;
      case 'w':
      case 'W': WriteEEPROM(); break;
      case 0: break; // empty string. Don't mind ignoring this.
      default: 
          Serial.print("ERR Unrecognised command");
          printACharacter(g_cmd[0]);
          Serial.println("") ; break;
    }
  }
}

void ReadEEPROM() // R<address> <length>  - read length bytes (in hex) from EEPROM, beginning at <address> (in hex)
{
/*
  if (g_cmd[1] == 0)
  {
    Serial.println("ERR no address");
    return;
  }
*/

  // decode ASCII representation of address (in hex) into an actual value
  
  long addr = 0;   // make address long in case using address like 0xD000
  int numberOfBytes = 0;
  int currentPosition = 1;

  while (currentPosition < 5 && g_cmd[currentPosition] != 0)
  {
    int nibble= HexToVal(g_cmd[currentPosition]);
    if (nibble>255){
      break;
    }
    addr = addr << 4;
    addr |= nibble;
    currentPosition+=1;
  }

  // check if length followed
  if (g_cmd[currentPosition] != 0){
    currentPosition +=1; // skip seperator
    while ( g_cmd[currentPosition] != 0)
    {
      int nibble= HexToVal(g_cmd[currentPosition]);
      if (nibble>255){
        break;
      }
      numberOfBytes = numberOfBytes << 4;
      numberOfBytes |= nibble;
      currentPosition+=1;
    }
    
  }
  if (numberOfBytes == 0 ) 
  {
    numberOfBytes=kMaxBufferSize;
  }
  /*
  Serial.print("address ");
  Serial.print(addr);
  Serial.print(" number ");
  Serial.print(numberOfBytes);
  Serial.println("");
  */
  

  while (numberOfBytes>0){
    int bytesToLoad;

    if (numberOfBytes>kOutputSize){
      bytesToLoad=kOutputSize;
      numberOfBytes -=kOutputSize;
    }
    else{
      bytesToLoad=numberOfBytes;
      numberOfBytes =0;
    }
    ReadEEPROMIntoBuffer(addr, bytesToLoad);
  
  
    PrintBuffer(addr, bytesToLoad);
    addr +=bytesToLoad;
  }
  Serial.println("OK");

}


// write data to the eeprom
// the data should be sent in the following format
// W07F0 42 07 C0 03 7D 03 78 00 FB
// W07F8 79 02 8E 00 62 03 B5 05 27
// W <four byte hex address> <data in hex, two characters per byte> space between each, max 16 plus cksum at end

void WriteEEPROM() 
{


  long addr = 0; // make address long in case using address like 0xD000
  int x = 1;
  int nibble1=0;
  int nibble2=0;
  // first skip any spaces 
  while (g_cmd[x] == ' ')
  {
    x++;
  }

  if (g_cmd[1] < 0X20)
  {
    Serial.println("ERR - W command needs address and data");
    return;
  }

  while (g_cmd[x] != ' ' && g_cmd[x] > 0X20 )
  {
    addr = addr << 4;
    nibble1 = HexToVal(g_cmd[x]);
    if (nibble1 > 255 ) {
      Serial.print("ERR unexpected character ");
      printACharacter(g_cmd[x]);
      Serial.println(" in address");
      return;
    }
    addr |= nibble1; 
    ++x;
  }

  // g_cmd[x] should now be a space
  if (g_cmd[x] != ' ')
  {
    Serial.println("ERR no data after address");
    return;
  }
  
  // x++; // now points to beginning of data
  // pointer should be at the space character
  uint8_t iBufferUsed = 0;

  // process hex pairs in the line until we find the 0 end marker
  // or a value less than 0x20 - nascom can put 0x08 markers on the end of the line
  // or until we have filled the buffer
  // we are assuming that the last hex pair on a line is the check sum
  // don't really expect more than 8 on a line but may get more
  
  while (true)
  {
    // first skip any spaces 
    while (g_cmd[x] == ' ')
    {
      x++;
    }
    // check if at end of input buffer
    // should be 0x0 or 0x08 
    
    if ( g_cmd[x] < 0x20 ){
      // if value less than a space treat as end of line
      break;
    }
    
    // ensure we don't overload the storage buffer
    if ( iBufferUsed > kMaxBufferSize ) {
      Serial.print("ERR more than '");
      Serial.print(kMaxBufferSize);
      Serial.println("' characters on input data ");
      return;
    }
    
    // a byte has 2 characters to make up a hex pair e.g. 4A
    // 4 is the first nibble and A is the second one 
    nibble1 = HexToVal(g_cmd[x]);
    if (nibble1 > 255 ) {
      Serial.print("ERR unexpected character ");
      printACharacter(g_cmd[x]);
      Serial.print(" in data nibble 1 of hexpair ");
      Serial.println(iBufferUsed + 1);
      return;
    }
    nibble2 = HexToVal(g_cmd[x+1]);
    if (nibble2 > 255 ) {
      Serial.print("ERR unexpected character ");
      printACharacter(g_cmd[x+1]);
      Serial.print(" in data nibble 2 of hex pair ");
      Serial.println(iBufferUsed + 1);
      return;
    }

    uint8_t c = (nibble1 << 4) | nibble2 ;
    // store in internal buffer and move on counter
    storageBuffer[iBufferUsed++] = c;
    
    x += 2 ; // step on past the hex pair processed

    if (g_cmd[x] > ' ')
    {
      Serial.print("ERR unexpected character ");
      printACharacter(g_cmd[x]);
      Serial.print(" after hex pair for byte ");
      Serial.println(iBufferUsed);
      return;
    }

    // Serial.println(c,HEX);

  } // end of filling buffer - should have data plus check sum

 
  // assume the checksum should be the last hex pair sent
  // remember iBufferUsed was added to after byte added
  byte checksum = storageBuffer[iBufferUsed-1];
  
  iBufferUsed -=1;  // substract one to give data count ( 

  if (iBufferUsed < 1)
  {
    // There is no data in the buffer after check sum removed
    iBufferUsed = -1;
    Serial.println("ERR only check sum sent no actual data bytes");
    return;
  }

  byte our_checksum = CalcBufferChecksum(addr, iBufferUsed);

  if (our_checksum != checksum)
  {
    // checksum fail!
    iBufferUsed = -1;
    Serial.print("ERR Checksum sent:");
    Serial.print(checksum, HEX);
    Serial.print(" generated:");
    Serial.print(our_checksum, HEX);
    Serial.println("");
    return;
  }

  PrintBuffer(addr, iBufferUsed);

  
  // buffer should now contains some data
  if (iBufferUsed > 0)
  {
    if (! (WriteBufferToEEPROM(addr, iBufferUsed))){
      Serial.print("ERR problem in write");
      return;
    }
  }

  if (iBufferUsed > -1)
  {
    Serial.println("OK");
  }
}



/*
 *  Sets or unsets the write protect status of the EEPROM - not checked this for the 28C16 chip ?
 */
// Important note: the EEPROM needs to have data written to it immediately after sending the "unprotect" command, so that the buffer is flushed.
// So we read byte 0 from the EEPROM first, then use that as the dummy write afterwards.
// It wouldn't matter if this facility was used immediately before writing an EEPROM anyway ... but it DOES matter if you use this option
// in isolation (unprotecting the EEPROM but not changing it).
void SetSDPState(bool bWriteProtect)
{

  SetRedLED();

  
  byte bytezero = ReadByteFrom(0);
  

  if (bWriteProtect)
  {
    WriteByteTo(0x1555, 0xAA);
    WriteByteTo(0x0AAA, 0x55);
    WriteByteTo(0x1555, 0xA0);
  }
  else
  {
    WriteByteTo(0x1555, 0xAA);
    WriteByteTo(0x0AAA, 0x55);
    WriteByteTo(0x1555, 0x80);
    WriteByteTo(0x1555, 0xAA);
    WriteByteTo(0x0AAA, 0x55);
    WriteByteTo(0x1555, 0x20);
  }
  
  WriteByteTo(0x0000, bytezero); // this "dummy" write is required so that the EEPROM will flush its buffer of commands.


  Serial.print("OK SDP ");
  if (bWriteProtect)
  {
    Serial.println("enabled");
  }
  else
  {
    Serial.println("disabled");
  }
  LedsOff();

}

// ----------------------------------------------------------------------------------------

/*
 * Reads bytes from the EEProm into the buffer area 
 */

void ReadEEPROMIntoBuffer(long addr, int size)
{
  
  for (int x = 0; x < size; ++x)
  {
    storageBuffer[x] = ReadByteFrom(addr + x);
  }

}

/*
 * writes bytes from the buffer area to the EEProm
 * 
 * returns false if the write fails
 */
bool WriteBufferToEEPROM(long addr, int size)
{
  for (uint8_t x = 0; x < size; ++x)
  {
    if (! WriteByteTo(addr + x, storageBuffer[x])){
      return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------------------------

/*  Reada a single byte from the EEProm
 *   
 *  sets the data lines as INPUTS, and 
 *  sets the control lines low and resets then high at the end
 *  
 *  Flashes the Yellow LED
 *  
 */

byte ReadByteFrom(long addr)
{
  SetYellowLED();

  SetDataLinesAsInputs();
  SetAddress(addr);
  digitalWrite(eeprom_nWE, HIGH); // disable write
  digitalWrite(eeprom_nOE, LOW); // EEPROM set to output
  digitalWrite(eeprom_nCE, LOW); // select eeprom
  delayMicroseconds(k_uTime_ReadPulse_uS);
  delay(10);
  // waitforserial();
  byte b = ReadData();
  digitalWrite(eeprom_nCE, HIGH); // deselect rom
  digitalWrite(eeprom_nOE, HIGH); // EEPROM disable output

  LedsOff();

  return b;
}




/*  write a byte to the eeprom 
 *    and waits to check DATA POLLING to ensure the write has completed
 *    
 *    Flashes the RED led
 *    
 *    return false if the write fails
 */
bool WriteByteTo(long addr, byte b){

  SetRedLED();

  //Serial.print("Addr:");
  //Serial.print(addr,HEX);
  //Serial.print("data:");
  //Serial.println(b,HEX);
  SetAddress(addr);
  SetDataLinesAsOutputs();
  SetData(b);

  // keep track of msb for polling write completion
  byte msb = b & 0x80;

  digitalWrite(eeprom_nOE, HIGH); // EEPROM no output
  digitalWrite(eeprom_nCE, LOW);
  digitalWrite(eeprom_nWE, LOW); // enable write  
  delayMicroseconds(k_uTime_WritePulse_uS);
  // waitforserial();
  digitalWrite(eeprom_nWE, HIGH); // disable write
  digitalWrite(eeprom_nCE, HIGH); 
  // Serial.println("checking write");
  // at this point a write should be in progress...
  // see https://www.reddit.com/r/beneater/comments/cc3o76/issues_programming_28c1620pc_200ns_eeproms_with/ 
  // poll for completion - changes the first bit on a read whenwrite is complete
  byte delayTime = 2;
  byte attempts = 0;
  byte pollBusy = ReadByteFrom( addr ) & 0x80;
  while ( pollBusy != msb )
  {
     delay(delayTime);
     pollBusy = ReadByteFrom( addr ) & 0x80;
     attempts++;
     // Serial.print(attempts);
     if (attempts>100){
       Serial.print("more than 100 attempts Data Polling at ");
       Serial.print("Addr:");
       Serial.print(addr,HEX);
       Serial.print("data:");
       Serial.println(b,HEX);
       LedsOff();
       // just carry on !!!
       // return false;
       break;
     }
  }
  
  LedsOff();

  return true;

}

// ----------------------------------------------------------------------------------------

/*
 * Sets the data line to input state ready for a read
 * This is where they are most of the time to avoid any conflict bwteen the EEProm and the Arduio
 * 
 */
void SetDataLinesAsInputs()
{
  pinMode(eepromData0, INPUT);
  pinMode(eepromData1, INPUT);
  pinMode(eepromData2, INPUT);
  pinMode(eepromData3, INPUT);
  pinMode(eepromData4, INPUT);
  pinMode(eepromData5, INPUT);
  pinMode(eepromData6, INPUT);
  pinMode(eepromData7, INPUT);
}

/*
 * Sets the data line to output state ready for a write
 * They should only be set as output just before a write and reset to read immediately afterwards
 * 
 */
void SetDataLinesAsOutputs()
{
  pinMode(eepromData0, OUTPUT);
  pinMode(eepromData1, OUTPUT);
  pinMode(eepromData2, OUTPUT);
  pinMode(eepromData3, OUTPUT);
  pinMode(eepromData4, OUTPUT);
  pinMode(eepromData5, OUTPUT);
  pinMode(eepromData6, OUTPUT);
  pinMode(eepromData7, OUTPUT);
}

/*
 * Sets the address lines to the value of the address parameter
 * It assumes address lines output status is set else where
 */
void SetAddress(long addressVal)
{
  // uses inline if and bitwise AND operator to check if bit set high or low
  digitalWrite(eepromAddress0,  (addressVal & 1   )?HIGH:LOW );
  digitalWrite(eepromAddress1,  (addressVal & 2   )?HIGH:LOW );
  digitalWrite(eepromAddress2,  (addressVal & 4   )?HIGH:LOW );
  digitalWrite(eepromAddress3,  (addressVal & 8   )?HIGH:LOW );
  digitalWrite(eepromAddress4,  (addressVal & 16  )?HIGH:LOW );
  digitalWrite(eepromAddress5,  (addressVal & 32  )?HIGH:LOW );
  digitalWrite(eepromAddress6,  (addressVal & 64  )?HIGH:LOW );
  digitalWrite(eepromAddress7,  (addressVal & 128 )?HIGH:LOW );
  digitalWrite(eepromAddress8,  (addressVal & 256 )?HIGH:LOW );
  digitalWrite(eepromAddress9,  (addressVal & 512 )?HIGH:LOW );
  digitalWrite(eepromAddress10, (addressVal & 1024)?HIGH:LOW );

}


/* 
 *  Sets the data output value
 * this function assumes that data lines output status is set elsewhere.
 * but even if they are not output at the time this will set the value for when they are switched to OUTPUT
 */
void SetData(byte byteVal)
{
  // uses inline if and bitwise AND operator to check if bit set high or low
  digitalWrite(eepromData0, (byteVal&1   )?HIGH:LOW );
  digitalWrite(eepromData1, (byteVal&2   )?HIGH:LOW );
  digitalWrite(eepromData2, (byteVal&4   )?HIGH:LOW );
  digitalWrite(eepromData3, (byteVal&8   )?HIGH:LOW );
  digitalWrite(eepromData4, (byteVal&16  )?HIGH:LOW );
  digitalWrite(eepromData5, (byteVal&32  )?HIGH:LOW );
  digitalWrite(eepromData6, (byteVal&64  )?HIGH:LOW );
  digitalWrite(eepromData7, (byteVal&128 )?HIGH:LOW );
}

/*  
 *   Reads the value present on the data pins
 *   this function assumes that data lines have already been set as INPUTS.
 */
byte ReadData()
{
  byte byteVal = 0;
  // uses bitwise OR operator to set bytes
  if (digitalRead(eepromData0) == HIGH) byteVal |= 1;
  if (digitalRead(eepromData1) == HIGH) byteVal |= 2;
  if (digitalRead(eepromData2) == HIGH) byteVal |= 4;
  if (digitalRead(eepromData3) == HIGH) byteVal |= 8;
  if (digitalRead(eepromData4) == HIGH) byteVal |= 16;
  if (digitalRead(eepromData5) == HIGH) byteVal |= 32;
  if (digitalRead(eepromData6) == HIGH) byteVal |= 64;
  if (digitalRead(eepromData7) == HIGH) byteVal |= 128;

  return(byteVal);
}

// ----------------------------------------------------------------------------------------

void PrintBuffer(long addr, int size)
{
  uint8_t chk = 0;
  chk += (addr & 0xff);
  chk += (addr >> 8) & 0xff;

  // now print the results, starting with the address as hex ...
  Serial.print(hex[ (addr & 0xF000) >> 12 ]);
  Serial.print(hex[ (addr & 0x0F00) >> 8  ]);
  Serial.print(hex[ (addr & 0x00F0) >> 4  ]);
  Serial.print(hex[ (addr & 0x000F)       ]);
  Serial.print(" ");

  for (uint8_t x = 0; x < size; ++x)
  {
    Serial.print(hex[ (storageBuffer[x] & 0xF0) >> 4 ]);
    Serial.print(hex[ (storageBuffer[x] & 0x0F)      ]);
    chk = chk + storageBuffer[x];
    if ( x < size - 1 ){
        Serial.print (" ");
    }
  }

  Serial.print(" ");
  Serial.print(hex[ (chk & 0xF0) >> 4 ]);
  Serial.print(hex[ (chk & 0x0F)      ]);
  Serial.println("");
}

void ReadString()
{
  int i = 0;
  byte c;

  g_cmd[0] = 0;
  do
  {
    if (Serial.available())
    {
      c = Serial.read();
      if (c > 31)
      {
        g_cmd[i++] = c;
        g_cmd[i] = 0;
      }
    }
  } 
  while (c != 10);
}

/* 
 *  debug routine to be used when checking stuff
 *  It waits for a newline on the serial port
 *  
 */
void waitforserial(){
    byte c;
    Serial.println("Waiting");
    while ( true ) {
      if ( Serial.available() ){
        c = Serial.read();
      }
      if (c==10) {
        break;
      }
    }

}

// sends a character to the serial output
// if standard ( 0x20 to 0x7E )  to then sends it and  prints the hex value
void printACharacter(byte dataByte){

  if (dataByte >= 0x20 && dataByte <= 0x7E ) {
    Serial.print("'");
    Serial.print(char(dataByte));
    Serial.print("'");
  }

  Serial.print("0x");
  Serial.print(dataByte,HEX);
  
}


// calculate the NAS check sum - which needs the address
// it justs add the values and does a mod 256 ( e.e. & 0xff )
uint8_t CalcBufferChecksum(long addr, uint8_t size)
{
  uint8_t chk = 0;

  chk += (addr & 0xff);
  chk += (addr >> 8) & 0xff;

  for (uint8_t x = 0; x < size; ++x)
  {
    chk = chk + storageBuffer[x];
  }
  chk = chk & 0xff; 
  return(chk);
}

// converts one character of a HEX value into its absolute value (nibble)
// returns 256 if not valid
int HexToVal(byte b)
{
  if (b >= '0' && b <= '9') return(b - '0');
  if (b >= 'A' && b <= 'F') return((b - 'A') + 10);
  if (b >= 'a' && b <= 'f') return((b - 'a') + 10);
  return(256);
}

// ------ led controls ----------------

void LedsOff(){
  // the 3 pins controlling LEDS
  pinMode(LEDPin1, INPUT);
  pinMode(LEDPin2, INPUT);
  pinMode(LEDPin3, INPUT);
  
}

void SetRedLED() {
  LedsOff();
  digitalWrite(LEDPin1,LOW);
  digitalWrite(LEDPin2,HIGH);
  pinMode(LEDPin1, OUTPUT);
  pinMode(LEDPin2, OUTPUT);
}

void SetYellowLED() {
  LedsOff();
  digitalWrite(LEDPin1,HIGH);
  digitalWrite(LEDPin2,LOW);
  pinMode(LEDPin1, OUTPUT);
  pinMode(LEDPin2, OUTPUT);

}

void SetGreenLED() {
  LedsOff();
  digitalWrite(LEDPin2,HIGH);
  digitalWrite(LEDPin3,LOW);
  pinMode(LEDPin2, OUTPUT);
  pinMode(LEDPin3, OUTPUT);
}
// ------------ end of code -------------
