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
#define DS	  17	//PC3
#define LATCH	18	//PC4
#define CLOCK	19	//PC5

//IO lines for RAM databus
//skip 2 as we need it for interrupts
#define D0 1	//PD1
#define D1 3	//PD3
#define D2 4	//PD4
#define D3 5	//PD5
#define D4 6	//PD6
#define D5 7	//PD7
#define D6 8	//PD8
#define D7 9	//PD9

//IO lines for RAM control
#define CE  14  //PC0; LED flashes for both Arduino CE* and TI MBE*
#define WE  16	//PC2

//define IO lines for TI99/4a control
#define TI_BUFFERS 	  15  //PC1; 74LS541 buffers enable/disable
#define TI_READY      0	 	//PD0; TI READY line + enable/disable 74HC595 shift registers
#define TI_INT      	2	  //PD2; 74LS138 interrupt (MBE*, WE* and A15) 

//interrupt routine flag for possible new FD1771 command
volatile byte FD1771 = 0;

//generic variable for R/W RAM
byte DSRAM  = 0;

byte ccmd         = 0;    //current command
byte lcmd         = 0;    //last command
boolean ncmd      = LOW;  //flag new command
byte ccruw        = 0;    //current CRUWRI value
byte lcruw        = 0;    //last CRUWRI value
int lrdi          = 0;    //byte counter for sector/track R/W
int secval        = 0;    //absolute sector number: (side * 359) + (track * 9) + WSECTR
long int btidx    = 0;    //absolute byte index: (secval * 256) + repeat R/W
  
boolean curdir = LOW; //current step direction, step in(wards) by default

#define RDINT   0x1FEA  //R6 counter value to detect read access in sector R/W, ReadID and track R/F
//CRU emulation bytes + FD1771 registers
#define CRURD   0x1FEC  //emulated 8 CRU input bits      (>5FEC in TI99/4a DSR memory block)
#define CRUWRI  0x1FEE  //emulated 8 CRU output bits     (>5FEE in TI99/4a DSR memory block)
#define RSTAT   0x1FF0  //read FD1771 Status register    (>5FF0 in TI99/4a DSR memory block)
#define RTRACK  0x1FF2  //read FD1771 Track register     (>5FF2 in TI99/4a DSR memory block)
#define RSECTR  0x1FF4  //read FD1771 Sector register    (>55F4 in TI99/4a DSR memory block)
#define RDATA   0x1FF6  //read FD1771 Data register      (>5FF6 in TI99/4a DSR memory block)
#define WCOMND  0x1FF8  //write FD1771 Command register  (>5FF8 in TI99/4a DSR memory block)
#define WTRACK  0x1FFA  //write FD1771 Track register    (>5FFA in TI99/4a DSR memory block)
#define WSECTR  0x1FFC  //write FD1771 Sector register   (>5FFC in TI99/4a DSR memory block)
#define WDATA   0x1FFE  //write FD1771 Data register     (>5FFE in TI99/4a DSR memory block)

//error blinking parameters: on (pwm), off, delay between sequence
#define LED_on      524288
#define LED_off     250
#define LED_repeat  1500

//input and output file pointers
File InDSR;   //DSR binary

//DSKx file pointers; x=1,2,3 for read, (x+3) for write
File DSK[7]; 

//flags for additional "drives" (aka DOAD files) available
boolean DSK2 = LOW;
boolean DSK3 = LOW;
byte    ADSK = 0;

//----- RAM, I/O data and control functions

//switch databus to INPUT state for TI RAM access 
void dbus_in() {
  DDRB &= B11111100;  //set PB1, PB0 to input (D7, D6)
  DDRD &= B00000101;  //set PD7-PD3 and PD1 to input (D5-D1, D0)
}

//switch databus to OUTPUT state so Arduino can play bus master
void dbus_out() {
  //set PB1, PB0, PD7-PD3 and PD1 to output
  DDRB |= B00000011;  //set PB1, PB0 to output (D7, D6)
  DDRD |= B11111010;  //set PD7-PD3 and PD1 to output (D5-D1, D0)
}

//disable Arduino control bus; CE* and WE* both HighZ
void dis_cbus() {
  pinAsInput(CE);
  pinAsInput(WE);
}
 
//enable Arduino control bus; CE* and WE* both HIGH
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
  PORTB |= ( (data >> 6) & B00000011); //write PB1, PBO (D7, D6)
  PORTD |= ( (data << 2) & B11111000); //write PD7-PD3 (D5-D1)
  PORTD |= ( (data << 1) & B00000010); //write PD1 (D0)
}

//shift out the given address to the 74HC595 registers
void set_abus(unsigned int address)
{
  //disable latch line
  digitalLow(LATCH);
  //clear data pin
  digitalLow(DS);

  //OK, repetitive, slightly ugly code but ... 1.53x as fast as the elegant for() - if() - else()
  //for every address bit (16) set: 
  //CLOCK -> LOW, address bit -> DS bit, CLOCK -> HIGH to shift and DS bit -> LOW to prevent bleed-through
  digitalLow(CLOCK);
    PORTC |= ( (address >> 12) & B00001000);  //D15
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address >> 11) & B00001000);  //D14
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address >> 10) & B00001000);  //D13
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address >>  9) & B00001000);  //D12
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address >>  8) & B00001000);  //D11
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address >>  7) & B00001000);  //D10
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address >>  6) & B00001000);  //D9
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address >>  5) & B00001000);  //D8
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address >>  4) & B00001000);  //D7
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address >>  3) & B00001000);  //D6
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address >>  2) & B00001000);  //D5
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address >>  1) & B00001000);  //D4
    digitalHigh(CLOCK);
    digitalLow(DS);
  digitalLow(CLOCK);
    PORTC |= ( (address      ) & B00001000);  //D3
    digitalHigh(CLOCK);
    digitalLow(DS); 
  digitalLow(CLOCK);
    PORTC |= ( (address <<  1) & B00001000);  //D2
    digitalHigh(CLOCK);
    digitalLow(DS); 
  digitalLow(CLOCK);
    PORTC |= ( (address <<  2) & B00001000);  //D1
    digitalHigh(CLOCK);
    digitalLow(DS); 
  digitalLow(CLOCK);
    PORTC |= ( (address <<  3) & B00001000);  //D0
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
  dis_cbus();               //cease Arduino RAM control
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
  //disable write
  digitalHigh(WE);
  //disable chip select
  digitalHigh(CE);
  //set databus to input
  dbus_in();
}

//flash error code
void eflash(byte error)
{
  //no APEDSK for you but TI still usable 
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
      //digitalHigh(CE);
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

  //TI99-4a I/O control
  pinAsOutput(TI_READY);
  pinAsOutput(TI_BUFFERS);
  //74LS138 interrupt: a write to DSR space (aka CRU, FD1771 registers, sector R/W (R6 counter in RAM) or track R/F (ditto))
  pinAsInput(TI_INT);

  //see if the SD card is present and can be initialized
  if (!SD.begin(SPI_CS)) {
    //nope -> flash LED error 1
    eflash(1);
  }

  //put TI on hold and enable 74HC595 shift registers
  TIstop();
  
  //check for existing DSR: read first DSR RAM byte ...
  DSRAM = Rbyte(0x0000); 
  // ... and check for valid DSR header mark (>AA)
  if ( DSRAM != 0xAA ) {
    //didn't find header so read DSR binary from SD and write into DSR RAM
    InDSR = SD.open("/APEDSK.DSR", FILE_READ);
    if (InDSR) {
      for (unsigned int ii = 0; ii < 0x2000; ii++) {
        DSRAM = InDSR.read();
        Wbyte(ii,DSRAM);
      }
    InDSR.close();
    }
    else {
      //couldn't open SD DSR file -> flash LED error 2
      eflash(2);
    }
  }

  //open DSK1 (error if doesn't exist)
  DSK[1] = SD.open("/DISKS/001.DSK", FILE_READ);
  if (!DSK[1]) {
    eflash(3);
  }
  else {
    DSK[1+3] = SD.open("/DISKS/001.DSK", FILE_WRITE);
  }
  
  //try to open DSK2 and flag if exists
  DSK[2] = SD.open("/DISKS/002.DSK", FILE_READ);
  if (DSK[2]) {
    DSK[2+3] = SD.open("/DISKS/002.DSK", FILE_WRITE);
    DSK2 = HIGH;
  }
   
  //try to open DSK3 and flag if exists
  DSK[3] = SD.open("/DISKS/003.DSK", FILE_READ);
  if (DSK[3]) {
    DSK[3+3] = SD.open("/DISKS/003.DSK", FILE_WRITE);
    DSK3 = HIGH;
  }
  
  //initialise FD1771 (clear CRU and FD1771 registers)
  for (unsigned int ii = CRURD; ii < (WDATA+1) ; ii++) {
    Wbyte(ii,0x00);
  } 
  
  //disable Arduino control bus, disable 74HC595 shift registers, enable TI buffers 
  TIgo(); 

  //enable TI interrupts (MBE*, WE* and A15 -> 74LS138 O0)
  attachInterrupt(digitalPinToInterrupt(TI_INT), listen1771, RISING);

} //end of setup()

void loop() {

  //check if flag has set by interrupt routine (TI WE*, MBE* and A15 -> 74LS138 O0)
  if (FD1771 == 0xBB) {

    noInterrupts(); //we don't want our interrupt be interrupted

    //first test if write was updating the CRU Write Register as we can go straight back to the TI
    if ( Rbyte(CRUWRI) == lcruw) {

      //nope so prep with reading Command Register
      DSRAM = Rbyte(WCOMND & 0xF0); //keep command only, strip unneeded floppy bits
        
      if ( DSRAM != lcmd ) {    //different to last command?
          ccmd = DSRAM;         //yes, set new command
          ncmd = HIGH;          //and flag it
      }
      else {
        ccmd = lcmd;            //no, continue with same command
      }
     
      if ( ccmd < 128 ) { //step/seek commands; no additional prep needed
      
        switch (ccmd) {
  	
         	case 0: //restore
            Wbyte(RTRACK,0);  //clear read track register
        		Wbyte(WTRACK,0);	//clear write track register        
        		Wbyte(WDATA,0); 	//clear write data register
        	break;
    	
        	case 16: //seek
        		//2 comparisons as if RTRACK == WDATA curdir doesn't change
            if ( Rbyte(RTRACK) > Rbyte(WDATA) ) { 
        		  curdir == LOW;  //step-in towards track 40
        		}	
        		else if ( Rbyte(RTRACK) < Rbyte(WDATA) ) { 
        		  curdir == HIGH; //step-out towards track 0
        		} 
        	  Wbyte(RTRACK,Rbyte(WDATA)); //update track register			
          break;
     
    	    case 32: //step
    	      //don't have to do anything for just step
    		  break;	    
    	
        	case 48: //step+T (update Track register)
        		DSRAM = Rbyte(RTRACK);	//read current track #
        		//is current direction inwards and track # still within limits?
        		if ( curdir == LOW && DSRAM < 39 ) {
        		  Wbyte(RTRACK,++DSRAM);  //increase track #
        		}
        		//is current direction outwards and track # still within limits?
        		else if ( curdir == HIGH && DSRAM >  0) {
        		  Wbyte(RTRACK,--DSRAM); //decrease track #
        		}			
          break;
         
      	  case 64: //step-in (towards track 39)
      		  curdir == LOW; //set current direction		
          break;
    	
        	case 80: //step-in+T (towards track 39, update Track Register)
        		DSRAM = Rbyte(RTRACK);  //read current track #
        		//if track # still within limits update track register
        		if ( DSRAM < 39) { 
        		  Wbyte(RTRACK,++DSRAM); 
        		}
        		curdir == LOW; //set current direction		
          break;
    
          case 96: //step-out (towards track 0)
      		  curdir == HIGH; //set current direction		
          break;
      
          case 112: //step-out+T
            DSRAM = Rbyte(RTRACK);  //read current track #
            //if track # still within limits update track register
            if ( DSRAM > 0) { 
              Wbyte(RTRACK,--DSRAM); 
            }
            curdir == HIGH; //set current direction    
          break;
      } //end switch seek/step commands

      else { //rest of commands; more prep needed

        if ( ncmd ) { //new sector read     
          //absolute sector calc: (side * 39) + (track * 9) + sector #
          secval = ( (Rbyte(CRURD) >> 7) * 39) + (Rbyte(RTRACK) * 9) + Rbyte(RSECTR); //calc absolute sector # in DOAD (0-359)
          btidx = secval * 256;                                                       //calc absolute byte index (0-92160)
          ADSK = Rbyte(CRURD) >> 7;                                                   //determine selected disk
          DSK[ADSK].seek(btidx);                                                      //set to first absolute byte
        }

        switch (ccmd) {
           
          case 128: //read sector
            DSRAM = DSK[ADSK].read();
            Wbyte(RDATA,DSRAM);
          break;
 /*      
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
      }
    }
  }
 }
  
  //reflect disk select + side select bits in CRU Read register. DISABLED for now
  //DSRAM = Rbyte(CRUWRI);
  //Wbyte(CRURD,( (DSRAM >> 4) & 0xE)) + (DSRAM & 0x80) );
  
  lcmd    = ccmd;               //save current command for compare in next interrupt
  ccmd    = 0;                  //ready to store next command (which could be more of the same)
  lcruw   = Rbyte(CRUWRI);      //save current CRU write register for compare in next interrupt
  lrdi    = Rbyte(RDINT+1);     //save current LSB R6 counter value; next byte in sector R/W, ReadID and track R/F 
  
  FD1771 = 0;   //clear interrupt flag
  interrupts(); //enable interrupts again

} //end of loop()

void listen1771() {
  noInterrupts();
  FD1771=0xBB;
}
