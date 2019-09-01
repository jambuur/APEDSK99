/* APEDISK99
* 
* This sketch emulates 3 TI99/4a disk drives. Shopping list:
*
* - Arduino Uno
*	- SD shield (RTC optional but potentially super handy)
*	- APEDSK99 shield
*
* The Arduino RAM interface is based on Mario's EEPROM programmer:
* https://github.com/mkeller0815/MEEPROMMER
*
* (much) faster digitalRead / Write by:
* http://masteringarduino.blogspot.com/2013/10/fastest-and-smallest-digitalread-and.html
* usage:
*   pinMode( pin, INPUT );        with pinAsInput( pin );
*   pinMode( pin, OUTPUT );       with pinAsOutput( pin );
*   pinMode( pin, INPUT_PULLUP);  with pinAsInputPullUp( pin );
*   digitalWrite( pin, LOW );     with digitalLow( pin );
*   digitalWrite( pin, HIGH );    with digitalHigh( pin );
*   digitalRead( pin )            with digitalState( pin )
*   if( digitalState( pin ) == HIGH )  can be if( isHigh( pin ) ) 
*   digitalState( pin ) == LOW         can be isLow( pin )
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

//faster digitalRead / digitalWrite definitions
#define portOfPin(P)\
  (((P)>=0&&(P)<8)?&PORTD:(((P)>7&&(P)<14)?&PORTB:&PORTC))
#define ddrOfPin(P)\
  (((P)>=0&&(P)<8)?&DDRD:(((P)>7&&(P)<14)?&DDRB:&DDRC))
#define pinOfPin(P)\
  (((P)>=0&&(P)<8)?&PIND:(((P)>7&&(P)<14)?&PINB:&PINC))
#define pinIndex(P)((uint8_t)(P>13?P-14:P&7))
#define pinMask(P)((uint8_t)(1<<pinIndex(P)))

#define pinAsInput(P) *(ddrOfPin(P))&=~pinMask(P)
#define pinAsInputPullUp(P) *(ddrOfPin(P))&=~pinMask(P);digitalHigh(P)
#define pinAsOutput(P) *(ddrOfPin(P))|=pinMask(P)
#define digitalLow(P) *(portOfPin(P))&=~pinMask(P)
#define digitalHigh(P) *(portOfPin(P))|=pinMask(P)
#define isHigh(P)((*(pinOfPin(P))& pinMask(P))>0)
#define isLow(P)((*(pinOfPin(P))& pinMask(P))==0)
#define digitalState(P)((uint8_t)isHigh(P))

//SPI and SD card functions
#include <SPI.h>
#include <SD.h>

//SPI card select pin
#define SPI_CS 10

//74HC595 shift-out definitions
#define DS	  17
#define LATCH	18
#define CLOCK	19

//IO lines for RAM databus
//skip 2 as we need it for interrupts
#define D0 1
#define D1 3
#define D2 4
#define D3 5
#define D4 6
#define D5 7
#define D6 8
#define D7 9

//IO lines for RAM control
#define CE  14  //LED flashes for both Arduino CE* and TI MBE*
#define WE  16

//define IO lines for TI99/4a control
#define TI_BUFFERS 	  15  //74LS541 buffers enable/disable
#define TI_READY      0	  //TI READY line + enable/disable 74HC595 shift registers
#define TI_INT      	2	  //74LS138 interrupt (MBE*, WE* and A15) 

//interrupt routine flag for possible new FD1771 command
volatile byte FD1771 = 0;

//generic variable for r/w RAM
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
#define LED_on      524288
#define LED_off     250
#define LED_repeat  1500

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
  DDRB = DDRB & B11111100;  //set PB1, PB0 to input
  DDRD = DDRD & B00000101;  //set PD7-PD3 and PD1 to input  
}

//switch databus to OUTPUT state
void dbus_out() {
  //set PB1, PB0, PD7-PD3 and PD1 to output
  DDRB = DDRB | B00000011;  //set PB1, PB0 to output
  DDRD = DDRD | B11111010;  //set PD7-PD3 and PD1 to output
}

//disable Arduino control bus
void dis_cbus() {
  pinAsInput(CE);
  pinAsInput(WE);
}
 
//enable Arduino control bus
void ena_cbus() {
  pinAsOutput(WE);
  digitalHigh(WE);  //default output state is LOW, could cause data corruption
  //delayMicroseconds(10); //CHECK: may cause data corruption
  pinAsOutput(CE);
  digitalLow(CE);   //default output state is LOW, could cause data corruption
}

//read a complete byte from the databus
//be sure to set databus to input first!
byte dbus_read() 
{   
  return( ((PINB & B00000011) << 6) +   //read PB1, PBO (D7, D6)
	        ((PIND & B11111000) >> 2) +   //read PD7-PD3 (D5-D1)
	        ((PIND & B00000010) >> 1));   //read PD1 (D0)
} 

//write a byte to the databus
//be sure to set databus to output first!
void dbus_write(byte data)
{
  PORTB = PORTB | ( (data >> 6) & B00000011); //write PB1, PBO (D7, D6)
  PORTD = PORTD | ( (data << 2) & B11111000); //write PD7-PD3 (D5-D1)
  PORTD = PORTD | ( (data << 1) & B00000010); //write PD1 (D0)
}

//shift out the given address to the 74HC595 registers
void set_abus(unsigned int address)
{
  //disable latch line
  digitalLow(LATCH);
  //clear data pin
  digitalLow(DS);

  //OK, repetitive, slightly ugly code but ... almost 2x as fast as a for() - if() - else()
  //for every address bit (16): 
  //CLOCK -> LOW, address bit -> DS bit, CLOCK -> HIGH to shift and DS bit -> LOW to prevent bleed-through
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >> 12) & B00001000);  //D15
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >> 11) & B00001000);  //D14
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >> 10) & B00001000);  //D13
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >>  9) & B00001000);  //D12
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >>  8) & B00001000);  //D11
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >>  7) & B00001000);  //D10
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >>  6) & B00001000);  //D9
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >>  5) & B00001000);  //D8
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >>  4) & B00001000);  //D7
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >>  3) & B00001000);  //D6
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >>  2) & B00001000);  //D5
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address >>  1) & B00001000);  //D4
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address      ) & B00001000);  //D3
    digitalHigh(CLOCK);
    digitalLow(DS); 
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address <<  1) & B00001000);  //D2
    digitalHigh(CLOCK);
    digitalLow(DS); 
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address <<  2) & B00001000);  //D1
    digitalHigh(CLOCK);
    digitalLow(DS); 
  digitalLow(CLOCK);
    PORTC = PORTC |  ( (address <<  3) & B00001000);  //D0
    digitalHigh(CLOCK);
    digitalLow(DS); 
   
  //stop shifting
  digitalLow(CLOCK);
  //enable latch and set address
  digitalHigh(LATCH);
}  

//short function to disable TI I/O
void TIstop()
{
   digitalLow(TI_READY);    //puts TI in wait state and enables 74HC595 shift registers
   digitalHigh(TI_BUFFERS); //disables 74LS541's
   ena_cbus();              //Arduino in control of RAM
}

//short function to enable TI I/O
void TIgo()
{
  dis_cbus();               //take RAM control from Arduino
  digitalLow(TI_BUFFERS);   //enable 74LS541's
  digitalHigh(TI_READY);    //wake up TI
}

//highlevel function to read a byte from a given address
byte Rbyte(unsigned int address)
{
  byte data = 0;
  //set address bus
  set_abus(address);
  //set databus for reading
  dbus_in();
  //enable RAM chip select
 digitalLow(CE);
  //get databus value
  data = dbus_read();
  //disable RAM chip select
  digitalHigh(CE);
  return data;
}

//highlevel function to write a byte to a given address
void Wbyte(unsigned int address, byte data)
{
  //set address bus
  set_abus(address);
  //set databus for writing
  dbus_out();
  //set data bus value
  dbus_write(data);
  //enable write
  digitalLow(CE);
  //enable RAM chip select
  digitalLow(WE);
  //disable chip select
  digitalHigh(WE);
  //disable write
  digitalHigh(CE);
  //set databus to input
  dbus_in();
}

//flash error code
void eflash(byte error)
{
  //TI still usable but no APEDSK for you
  digitalHigh(TI_BUFFERS);
  digitalHigh(TI_READY);
    
  //make sure we can toggle Arduino CE* to flash the error code
  pinAsOutput(CE);

  //stuck in error flashing loop until reset
  while (1) {

    for (byte flash = 0; flash < error; flash++)
    {
      //poor man's PWM, reduce mA's through LED to prolong its life
      for (unsigned long int ppwm =0; ppwm < LED_on; ppwm++) {
        digitalLow(CE);  //turn on RAM CE* and thus LED
        digitalHigh(CE); //turn it off
      }
      //disable RAM CE*, turning off LED
      digitalHigh(CE);
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
  pinAsOutput(DS);
  pinAsOutput(LATCH);
  pinAsOutput(CLOCK);

  //TI99/4a I/O control
  pinAsOutput(TI_READY);
  pinAsOutput(TI_BUFFERS);
  //74LS138 interrupt: write to DSR space (aka CRU or FD1771 registers)
  pinAsInput(TI_INT);

  //see if the SD card is present and can be initialized
  if (!SD.begin(SPI_CS)) {
    //nope -> flash LED error 1
    eflash(1);
  }

  //put TI on hold and enable 74HC595 shift registers
  ena_cbus();
  TIstop();
  
  //check for existing DSR: read first DSR RAM byte ...
  DSRAM = Rbyte(0x0000); 
  // ... and check for valid DSR header (>AA)
  if ( DSRAM != 0xAA ) {
    //didn't find header so read DSR binary from SD and write into DSR RAM
    InDSR = SD.open("/APEDSK.DSR", FILE_READ);
    if (InDSR) {
      for (unsigned int CByte =0; CByte < 0x2000; CByte++) {
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
  for (unsigned int CByte = CRURD; CByte < (WDATA+1) ; CByte++) {
    Wbyte(CByte,0x00);
  } 
  
  //disable Arduino control bus 
  dis_cbus();
  //enable TI buffers, disable 74HC595 shift registers
  TIgo(); 

  //enable TI interrupts (MBE*, WE* and A15 -> 74LS138 O0)
  //attachInterrupt(digitalPinToInterrupt(TI_INT), listen1771, RISING);

  byte ccmd =  0; //current command
  byte lcmd =  0; //former command
  byte lcruw = 0; //former CRUWRI value
  int lr6val = 0; //byte counter for sector/track R/W
  int secval = 0; //absolute sector number: (side * 359) + (track * 9) + WSECTR
  
  boolean curdir = LOW; //current step direction, step in by default

}

void loop() {

  //if (FD1771 == 0xBB) {

    /*ccmd = Rbyte(WCOMND) & 0xF; //strip floppy-specific bits we don't need, keep command only
    
    switch (ccmd) {
	
    	case 0: //restore
    		Wbyte(WTRACK,0);	//clear write track register        
    		Wbyte(WDATA,0); 	//clear write data register
    		Wbyte(RTRACK,0);	//clear read track register
    	break;
	
    	case 16: //seek
    		//2 comparisons because if RTRACK == WDATA curdir doesn't change
    		if ( Rbyte(RTRACK) > Rbyte(WDATA) ) { curdir == LOW;  }	//step-in towards track 40
    		if ( Rbyte(RTRACK) < Rbyte(WDATA) ) { curdir == HIGH; }	//step-out towards track 0
    	  Wbyte(RTRACK,Rbyte(WDATA)); //update track register			
      break;
	
	    case 32: //step
	      //don't have to do anything for just step
		  break;	    
	
    	case 48: //step+T
    		DSRAM = Rbyte(RTRACK);	//read current track #
    		//is current direction inwards and track # still within limits?
    		if ( DSRAM < 39 && curdir == LOW ) { Wbyte(RTRACK,DSRAM++); }	//yes, increase track #
    		//is current direction outwards and track # still within limits?
    		if ( DSRAM >  0 && curdir == HIGH) { Wbyte(RTRACK,DSRAM--); }	//yes, decrease track #		
      break;
	
  	  case 64: //step-in
  		  curdir == LOW; //set current direction		
      break;
	
    	case 80: //step-in+T
    		DSRAM = Rbyte(RTRACK);  //read current track #
    		//if track # still within limits update track register
    		if ( DSRAM < 39) { Wbyte(RTRACK,DSRAM++); }
    		curdir == LOW; //set current direction		
      break;

      case 96: //step-out
  		  curdir == HIGH; //set current direction		
      break;
  
      case 112: //step-out+T
        DSRAM = Rbyte(RTRACK);  //read current track #
        //if track # still within limits update track register
    		if ( DSRAM > 0) { Wbyte(RTRACK,DSRAM--); }
    		curdir == LOW; //set current direction		
      break;
/*
	    case 128: //read sector
       		if ( ccmd != lcmd ) { //new sector read
			secval = (side * 359) + (track * 9) + WSECTR
			
 while m is set AND WSECTR < max
          while RDINT >= 0
          sector byte -> RDATA
        update mark type
        update RSECTR
	      break;
	    case 144: //read multiple sectors
	      break;
	    case 160: //write sector
        if P then
         set p bit, abort
       else
        while m is set AND WSECTR < max
          while RDINT >= 0
          RDATA -> sector byte
        update RSECTR
	      break;
	    case 176: //write multiple sectors
	      break;
	    case 192: //read ID
        RTRACK -> WDATA
        SIDE -> WDATA
        RSECTR -> WDATA
        sector length -> WDATA
        CRC x2 -> WDATA
	      break;
	    case 208: //force interrupt
	      break;
	    case 224: //read track
        while sector <  sectors p/t
          while bytes < 256
          sector byte -> RDATA
        next sector
	      break;
	    case 240: //write track
        while sector <  sectors p/t
	      break; */
  
  //}
  //FD1771 = 0;  
  //interrupts(); 
}
void listen1771() {
  noInterrupts();
  FD1771=0xBB;
}
