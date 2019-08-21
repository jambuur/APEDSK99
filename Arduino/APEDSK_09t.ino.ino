/* APEDISK99
* 
* This sketch emulates 3 TI99/4a disk drives. It will need:
*
* 	- Arduino Uno
*	- SD shield (RTC optional but potentially super handy)
*	- APEDSK99 shield
*
* The Arduino RAM interface is based on Mario's EEPROM programmer:
* https://github.com/mkeller0815/MEEPROMMER
*
* $Author: Jochen Buur $
* $Date: Aug 2019 $
* $Revision: 0.12k $
*
* This software is freeware and can be modified, re-used or thrown away without any restrictions.
*
* Use this code at your own risk. 
* I'm not responsible for any bad effect or damages caused by this software
*/

//SPI and SD card functions
#include <SPI.h>
#include <SD.h>

//SPI card select pin
#define SPI_CS 10

//74HC595 shift-out definitions
#define DS	A3
#define LATCH	A4
#define CLOCK	A5

#define PORTC_DS	3
#define PORTC_LATCH	4
#define PORTC_CLOCK	5

//define IO lines for RAM databus
//skip 2 as we need it for interrupts
#define D0 1
#define D1 3
#define D2 4
#define D3 5
#define D4 6
#define D5 7
#define D6 8
#define D7 9

//define IO lines for RAM control
#define CE		A0	//LED flashes for both Arduino CE* and TI MBE*
#define WE		A2
#define PORTC_CE   	0
#define PORTC_WE   	2

//define IO lines for TI99/4a control
#define TI_BUFFERS 	A1	//74LS541 buffers enable/disable
#define TI_READY    	0	//TI READY line + enable/disable 74HC595 shift registers
#define TI_INT      	2	//74LS138 interrupt (MBE*, WE* and A15) 

//flag from interrupt routine to signal possible new FD1771 command
volatile byte FD1771 = 0;

//generic variable for r/w DSRAM
byte DSRAM = 0;

//CRU emulation bytes + FD1771 registers
#define CRURD   0x1FEC     //emulated 8 CRU input bits
#define CRUWRI  0x1FEE     //emulated 8 CRU output bits
#define RSTAT   0x1FF0     //read FD1771 Status register
#define RTRACK  0x1FF2     //read FD1771 Track register
#define RSECTR  0x1FF4     //read FD1771 Sector register
#define RDATA   0x1FF6     //read FD1771 Data register
#define WCOMND  0x1FF8     //write FD1771 Command register
#define WTRACK  0x1FFA     //write FD1771 Track register
#define WSECTR  0x1FFC     //write FD1771 Sector register
#define WDATA   0x1FFE     //write FD1771 Data register

//error blinking parameters: on, off, delay between sequence
#define LED_on  350
#define LED_off 250
#define LED_repeat 1500

//input and output file pointers
File InDSR;
File InDSK1;
File OutDSK1;
File InDSK2;
File OutDSK2;
File InDSK3;
File OutDSK3;

//flags for additional "drives" available
boolean DSK2 = LOW;
boolean DSK3 = LOW;

//----- RAM, I/O data and control functions

//switch databus to INPUT state
void dbus_in() {
  pinMode(D0, INPUT);
  pinMode(D1, INPUT);
  pinMode(D2, INPUT);
  pinMode(D3, INPUT);
  pinMode(D4, INPUT);
  pinMode(D5, INPUT);
  pinMode(D6, INPUT);
  pinMode(D7, INPUT);
}

//switch databus to OUTPUT state
void dbus_out() {
  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);
}

//disable Arduino control bus
void dis_cbus() {
  pinMode(CE, INPUT);
  pinMode(WE, INPUT);
}
 
//enable Arduino control bus
void ena_cbus() {
  pinMode(CE, OUTPUT);
  pinMode(WE, OUTPUT);
}

//read a complete byte from the databus
//be sure to set databus to input first!
byte dbus_read()
{
  return ((digitalRead(D7) << 7) +
          (digitalRead(D6) << 6) +
          (digitalRead(D5) << 5) +
          (digitalRead(D4) << 4) +
          (digitalRead(D3) << 3) +
          (digitalRead(D2) << 2) +
          (digitalRead(D1) << 1) +
           digitalRead(D0));
}

//write a byte to the databus
//be sure to set databus to output first!
void dbus_write(byte data)
{
  digitalWrite(D7, (data >> 7) & 0x01);
  digitalWrite(D6, (data >> 6) & 0x01);
  digitalWrite(D5, (data >> 5) & 0x01);
  digitalWrite(D4, (data >> 4) & 0x01);
  digitalWrite(D3, (data >> 3) & 0x01);
  digitalWrite(D2, (data >> 2) & 0x01);
  digitalWrite(D1, (data >> 1) & 0x01);
  digitalWrite(D0, data & 0x01);  
}

//shift out the given address to the 74HC595 registers
void set_abus(unsigned int address)
{
  //get MSB of 16 bit address
  byte MSB = address >> 8;
  //get LSB of 16 bit address
  byte LSB = address & 0xFF;

  //disable latch line
  bitClear(PORTC,PORTC_LATCH);

  //shift out MSB byte
  fastShiftOut(MSB);
  //shift out LSB byte
  fastShiftOut(LSB);

  //enable latch and set address
  bitSet(PORTC,PORTC_LATCH);
}

//faster shiftOut function then normal IDE function (about 4 times)
void fastShiftOut(byte data) {
  //clear data pin
  bitClear(PORTC,PORTC_DS);
  //send each bit of the data byte; MSB first
  for (int i=7; i>=0; i--)  {
    bitClear(PORTC,PORTC_CLOCK);
    //Turn data on or off based on value of bit
    if ( bitRead(data,i) == 1 ) {
      bitSet(PORTC,PORTC_DS);
    }
    else {      
      bitClear(PORTC,PORTC_DS);
    }
    //register shifts bits on upstroke of clock pin  
    bitSet(PORTC,PORTC_CLOCK);
    //zero the data pin after shift to prevent bleed through
    bitClear(PORTC,PORTC_DS);
  }
  //stop shifting
  bitClear(PORTC,PORTC_CLOCK);
}

//short function to set RAM CE* (OE* and CS) are permanenty enabled)
void set_CE (byte state)
{
  digitalWrite(CE, state);
}

//short function to set RAM WE* (Write Enable)
void set_WE (byte state)
{
  digitalWrite(WE, state);
}

//short function to enable/disable TI interface 74LS541 buffers
void TIbuf (byte state)
{
  digitalWrite(TI_BUFFERS, state);
}

//short function to set TI READY line (pause/resume TI); 74HC595 buffers are !set at the same time
void TIready (byte state)
{
  digitalWrite(TI_READY, state);
}

//short function to disable TI I/O
void TIstop()
{
  TIready(LOW);
  TIbuf(HIGH);
}

//short function to enable TI I/O
void TIgo()
{
  TIbuf(LOW);
  TIready(HIGH);
}

//highlevel function to read a byte from a given address
byte Rbyte(unsigned int address)
{
  byte data = 0;
 
  //disable RAM write
  set_WE(HIGH);
  //set address bus
  set_abus(address);
  //set databus for reading
  dbus_in();
  //enable RAM chip select
  set_CE(LOW);
  //get databus value
  data = dbus_read();
  //disable RAM chip select
  set_CE(HIGH);
 
  return data;
}

//highlevel function to write a byte to a given address
void Wbyte(unsigned int address, byte data)
{
  //set address bus
  set_abus(address);
  //set databus for writing
  dbus_out();
  //enable RAM chip select
  set_CE(LOW);
  //enable write
  set_WE(LOW);
  //set data bus value
  dbus_write(data);
  //disable write
  set_WE(HIGH);
  //disable chip select
  set_CE(HIGH);
  //set databus to input
  dbus_in();
}

//flash error code
void eflash(byte error)
{
  //TI still usable but no APEDSK for you
  TIbuf(HIGH);
  TIready(HIGH);
    
  //make sure we can toggle Arduino CE* to flash the error code
  pinMode(CE, OUTPUT);

  //stuck in error flashing loop until reset
  while (1) {

    for (byte flash = 0; flash < error; flash++)
    {
      //enable RAM for Arduino, turns on LED
      set_CE(LOW);
      //LED is on for a bit
      delay(LED_on);
      //disable RAM for Arduino, turns off LED
      set_CE(HIGH);
      //LED is off for a bit
      delay(LED_off);
    } 
      //allow human error interpretation
      delay(LED_repeat);
  }
}

//----- Main
  
void setup() {

  //74HC595 shift register control
  pinMode(DS, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode(CLOCK, OUTPUT);

  //TI99/4a I/O control
  pinMode(TI_READY, OUTPUT);
  pinMode(TI_BUFFERS, OUTPUT);
  //74LS138 interrupt: write to DSR space (aka CRU or FD1771 registers)
  pinMode(TI_INT, INPUT);

  //see if the SD card is present and can be initialized
  if (!SD.begin(SPI_CS)) {
    //nope -> flash LED error 1
    eflash(1);
  }

  //put TI on hold and enable 74HC595 shift registers
  TIstop();
  //enable Arduino control bus
  ena_cbus();
  
  //check for existing DSR: read first DSR RAM byte (>4000 in TI address space) ...
  DSRAM = Rbyte(0x0000);
  
  // ... and check for valid DSR header (>AA)
  if ( DSRAM != 0xAA ) {
    //didn't find header so read DSR binary from SD and write into DSR RAM
    InDSR = SD.open("/APEDSK.DSR", FILE_READ);
    if (InDSR) {
      for (int CByte =0; CByte < 0x2000; CByte++) {
        DSRAM = InDSR.read();
        Wbyte(CByte,DSRAM);
      }
    InDSR.close();
    }
    else {
      //couldn't open SD DSR file -> flash LED error 2
      eflash(2);
    }
  }

  //open DSK1 (error if doesn't exist)
  InDSK1 = SD.open("/DISKS/001.DSK", FILE_READ);
    if (!InDSK1) {
      eflash(3);
    }
  //try to open DSK2 and flag if exists
  InDSK2 = SD.open("/DISKS/002.DSK", FILE_READ);
    if (InDSK2) {
      DSK2 = HIGH;
    }
  //try top open DSK3 and flag if exists
  InDSK3 = SD.open("/DISKS/003.DSK", FILE_READ);  
    if (InDSK3) {
      DSK3 = HIGH;
    }

  //initialise FD1771 (clear CRU and FD1771 registers)
  for (int CByte = CRURD; CByte < WDATA+2; CByte++) {
    Wbyte(CByte,0);
  }
 
  //disable Arduino control bus 
  dis_cbus();
  //enable TI buffers, disable 74HC595 shift registers
  TIgo(); 

  //enable TI interrupts (MBE*, WE* and A15 -> 74LS138 O0)
  attachInterrupt(digitalPinToInterrupt(TI_INT), listen1771, RISING);
}

void loop() {

  if (FD1771 == 0xBB) {

/*

  //pseudo
	Step	
		determine current direction
		execute Step-in or Step-out
	Step In	
		set direction inward
		if T then update Treg
	Step Out	
		set direction outward
		if T then update Treg
	Seek	
		WTRACK = WDATA
	Restore	
		WTRACK, WDATA = 0
	Read Sector	
		while m is set AND WSECTR < max
			while RDINT >= 0
			sector byte -> RDATA
			update mark type
		update RSECTR
	Write Sector	
		if P then
			set p bit, abort
		else
		while m is set AND WSECTR < max
			while RDINT >= 0
			RDATA -> sector byte
		update RSECTR
	Read ID	
		RTRACK -> WDATA
		SIDE -> WDATA
		RSECTR -> WDATA
		sector length -> WDATA
		CRC x2 -> WDATA
	Read Track	
		while sector <  sectors p/t
			while bytes < 256
				sector byte -> RDATA
		next sector
	Write Track	
		while sector <  sectors p/t
*/    

  FD1771 = 0;  
  interrupts(); 
  } 
}

void listen1771() {
  noInterrupts();
  FD1771=0xBB;
}
