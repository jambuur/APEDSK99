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

//----- definitions, functions and initialisations

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

//IO lines for Arduino RAM control
#define CE  14  //PC0  LED flashes for both Arduino CE* and TI MBE*
#define WE  16	//PC2

//IO lines for TI99/4a control
#define TI_BUFFERS 	  15  //PC1; 74LS541 buffers enable/disable
#define TI_READY      0	 	//PD0; TI READY line + enable/disable 74HC595 shift registers
#define TI_INT      	2	  //PD2; 74LS138 interrupt (MBE*, WE* and A15) 

//R6 counter value to detect read access in sector, ReadID and track commands
#define RDINT   0x1FEA  

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

//switch databus to INPUT state for TI RAM access 
void dbus_in() {
  DDRD  &= B00000101;  //set PD7-PD3 and PD1 to input (D5-D1, D0) 
  PORTD &= B00000101;  //initialise input pins
  DDRB  &= B11111100;  //set PB1, PB0 to input (D7, D6)
  PORTB &= B11111100;  //initialise input pins
}

//switch databus to OUTPUT state so Arduino can play bus master
void dbus_out() {
  DDRD  |= B11111010;  //set PD7-PD3 and PD1 to output (D5-D1, D0)
  DDRB  |= B00000011;  //set PB1, PB0 to output (D7, D6)
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
  pinAsOutput(CE);
  digitalHigh(CE);   //default output state is LOW, could cause data corruption
}

//read a byte from the databus
byte dbus_read() 
{   
  delayMicroseconds(2);                 //long live the Logic Analyser
  return( ((PIND & B00000010) >> 1) +   //read PD1 (D0)
          ((PIND & B11111000) >> 2) +   //read PD7-PD3 (D5-D1)
          ((PINB & B00000011) << 6) );  //read PB1, PBO (D7, D6) 
 } 

//place a byte on the databus
void dbus_write(byte data)
{
  PORTD |= ((data << 1) & B00000010); //write PD1 (D0)
  PORTD |= ((data << 2) & B11111000); //write PD7-PD3 (D5-D1)
  PORTB |= ((data >> 6) & B00000011); //write PB1, PBO (D7, D6)
}

//shift out the given address to the 74HC595 registers
void set_abus(unsigned int address)
{
  //disable latch line
  digitalLow(LATCH);
  //clear data pin
  digitalLow(DS);

  //OK, repetitive, slightly ugly code but ... 1.53x as fast as the more elegant for() - if() - else()
  //for every address bit (13 bits to address 8Kbytes) set: 
  //CLOCK -> LOW, address bit -> DS bit, CLOCK -> HIGH to shift and DS bit -> LOW to prevent bleed-through
  digitalLow(CLOCK);
    PORTC |= ( (address >>  9) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D12   
  digitalLow(CLOCK);
    PORTC |= ( (address >>  8) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D11 
  digitalLow(CLOCK);
    PORTC |= ( (address >>  7) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D10
  digitalLow(CLOCK);
    PORTC |= ( (address >>  6) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D9
  digitalLow(CLOCK);
    PORTC |= ( (address >>  5) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D8
  digitalLow(CLOCK);
    PORTC |= ( (address >>  4) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D7
  digitalLow(CLOCK);
    PORTC |= ( (address >>  3) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D6
  digitalLow(CLOCK);
    PORTC |= ( (address >>  2) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D5
  digitalLow(CLOCK);
    PORTC |= ( (address >>  1) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D4
  digitalLow(CLOCK);
    PORTC |= ( (address      ) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D3
  digitalLow(CLOCK);
    PORTC |= ( (address <<  1) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D2
  digitalLow(CLOCK);
    PORTC |= ( (address <<  2) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D1
  digitalLow(CLOCK);
    PORTC |= ( (address <<  3) & B00001000); digitalHigh(CLOCK); digitalLow(DS); //D0
   
  //stop shifting
  digitalLow(CLOCK);
  //enable latch and set address
  digitalHigh(LATCH);
}  

//disable TI I/O, enable Arduino shift registers and control bus
void TIstop()
{
   digitalHigh(TI_BUFFERS); //disables 74LS541's
   pinAsOutput(TI_READY);   //switch from HighZ to output
   digitalLow(TI_READY);    //puts TI in wait state and enables 74HC595 shift registers
   ena_cbus();              //Arduino in control of RAM
}

//enable TI I/O, disable Arduino shift registers and control bus
void TIgo()
{
  dis_cbus();               //cease Arduino RAM control
  pinAsInput(TI_READY);     //switch from output to HighZ: disables 74HC595's and wakes up TI
  digitalLow(TI_BUFFERS);   //enable 74LS541's 
}

//read a byte from RAM address
byte Rbyte(unsigned int address)
{
  byte data = 0x00;
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

//write a byte to RAM address
void Wbyte(unsigned int address, byte data)
{
  //set address bus
  set_abus(address);
  //set databus for writing
  dbus_out();
  //set data bus value
  dbus_write(data);
  //enable write
  digitalLow(WE);
  //enable RAM chip select
  digitalLow(CE);
  //disable chip select
  digitalHigh(CE);
  //disable write
  digitalHigh(WE);
  //set databus for reading
  dbus_in();
}

//flash error code
void eflash(byte error)
{
  //no APEDSK99 for you but TI console still usable
  digitalHigh(TI_BUFFERS);
  digitalHigh(TI_READY);
    
  //make sure we can toggle Arduino CE* to flash the error code
  pinAsOutput(CE);

  //error routine: stuck in code flashing loop until reset
  while (1) {

    for (byte flash = 0; flash < error; flash++)
    {
      //poor man's PWM, reduce mA's through LED to prolong its life
      for (unsigned long int ppwm = 0; ppwm < LED_on; ppwm++) {
        digitalLow(CE);  //set RAM CE* LOW, turns on LED
        digitalHigh(CE); //turn it off
      }
      //LED is off for a bit
      delay(LED_off);
    } 
      //allow human error interpretation
      delay(LED_repeat);
  }
}

//set Track 0 bit in Status Register
void sTrack0(byte track) {
  if ( track == 0 ) {
    Wbyte(RSTAT, Rbyte(RSTAT) | B00000100); //set Track 0 bit in Status Register;
  }
  else {
    Wbyte(RSTAT, Rbyte(RSTAT) & B11111011); //reset Track 0 bit in Status Register;  
 }
} 

//interrupt routine flag: possible new / continued FD1771 command
volatile byte FD1771 = 0;

//generic variable for RAM R/W
byte DSRAM  = 0;

//DSR binary input file pointer
File InDSR;  

//DSKx file pointers; x=1,2,3 for read, (x+3) for write
File DSK[7]; 	//read and write file pointers to DOAD's
#define woff 3	//write file pointer (array index offset from read file pointer)

//flags for "drives" (aka DOAD files) available (DSK1 should always be available, if not Flash Error 3)
boolean aDSK[4] = {LOW,HIGH,LOW,LOW};
//current selected DSK
byte cDSK = 0;
//protected DSK flag
boolean pDSK = LOW;

//various storage and flags for command interpretation and handling
byte ccmd         = 0;    //current command
byte lcmd         = 0;    //last command
boolean ncmd      = LOW;  //flag new command
byte ccruw        = 0;    //current CRUWRI value
byte lcruw        = 0;    //last CRUWRI value
int lrdi          = 0;    //byte counter for sector/track R/W
int secval        = 0;    //absolute sector number: (side * 359) + (track * 9) + WSECTR
long int btidx    = 0;    //absolute DOAD byte index: (secval * 256) + repeat R/W
boolean curdir    = LOW;  //current step direction, step in(wards) towards track 39 by default

//------------  
void setup() {

 //see if the SD card is present and can be initialized
  if (!SD.begin(SPI_CS)) {
    //nope -> flash LED error 1
    eflash(1); 
  } 
	
  //74HC595 shift register control
  pinAsOutput(DS);
  pinAsOutput(LATCH);
  pinAsOutput(CLOCK);

  //74LS138 interrupt: TI WE*, MBE* and A15 -> O0 output
  //a write to DSR space (aka CRU, FD1771 registers; or sector/track Read through R6 counter in RAM, see DSR source)
  pinAsInput(TI_INT);
	
  //TI99-4a I/O control
  pinAsOutput(TI_READY);
  pinAsOutput(TI_BUFFERS);
  
  //put TI on hold and enable 74HC595 shift registers
  TIstop();
 
  //read DSR binary from SD and write into DSR RAM
  InDSR = SD.open("/APEDSK.DSR", FILE_READ);
  if (InDSR) {
    for (unsigned int ii = 0; ii < 0x2000; ii++) {
      DSRAM = InDSR.read();
      Wbyte(ii,DSRAM);
    }
    InDSR.close();
  }
  //check for DSR header at first DSR RAM byte ...
  DSRAM = Rbyte(0x0000); 
  // ... and check for valid mark (>AA)
  if ( DSRAM != 0xAA ) {
    //loading DSR unsuccessful -> flash LED error 2
    eflash(2);
  }

 //try to open DSK1 for reads
 DSK[1] = SD.open("/DISKS/001.DSK", FILE_READ);
  if ( !DSK[1] ) {
    eflash(3);	//could not open DSK1 -> flash error 3
  }
  else {
    //DSK1 is available, assign file pointer for writes	  
    DSK[1+woff] = SD.open("/DISKS/001.DSK", FILE_WRITE);
  }
  
  //try to open DSK2 for reads
  DSK[2] = SD.open("/DISKS/002.DSK", FILE_READ);
  if ( DSK[2] ) {
    //DSK2 is available, assign file pointer for writes ...
    DSK[2+woff] = SD.open("/DISKS/002.DSK", FILE_WRITE);
    //... and set flag
    aDSK[2] = HIGH;
  }
   
  //try to open DSK3 for reads
  DSK[3] = SD.open("/DISKS/003.DSK", FILE_READ);
  if ( DSK[3] ) {
    //DSK3 is available, assign file pointer for writes ...
    DSK[3+woff] = SD.open("/DISKS/003.DSK", FILE_WRITE);
    //... and set flag
    aDSK[3] = HIGH;
  }
  
  //initialise FD1771 (clear "CRU" and "FD1771 registers")
  for (unsigned int ii = CRURD; ii < (WDATA+1) ; ii++) {
    Wbyte(ii,0x00); 
  } 

  //initialise Status Register: set Head Loaded and Track 0 bits
  Wbyte(RSTAT,B00100100); 

  FD1771 = 0;   //clear interrupt flag
  //enable TI interrupts (MBE*, WE* and A15 -> 74LS138 O0)
  attachInterrupt(digitalPinToInterrupt(TI_INT), listen1771, RISING);

  //disable Arduino control bus, disable 74HC595 shift registers, enable TI buffers 
  TIgo();  

} //end of setup()

void loop() {

  //check if flag has set by interrupt routine 
  if (FD1771 == 0xBB) {
    
    DSRAM = Rbyte(WCOMND) & 0xF0; //keep command only, strip unneeded floppy bits

    Wbyte(0x1FDE,DSRAM);

    switch (DSRAM) {

      case 0:
        Wbyte(0x1FE0,0);
      break;
     case 16:
        Wbyte(0x1FE0,16);
      break;
    case 32:
        Wbyte(0x1FE0,32);
      break;
    case 48:
        Wbyte(0x1FE0,48);
      break;
    case 64:
        Wbyte(0x1FE0,64);
      break;
    case 80:
        Wbyte(0x1FE0,80);
      break;
    case 96:
        Wbyte(0x1FE0,96);
      break;
    case 112:
        Wbyte(0x1FE0,112);
      break;
    case 128:
        Wbyte(0x1FE0,128);
      break;
    case 144:
        Wbyte(0x1FE0,144);
      break;
    case 160:
        Wbyte(0x1FE0,160);
      break;
    case 174:
        Wbyte(0x1FE0,174);
      break;
    case 192:
        Wbyte(0x1FE0,192);
      break;
    case 208:
        Wbyte(0x1FE0,208);
      break;
    case 224:
        Wbyte(0x1FE0,224);
      break;
    case 240:
        Wbyte(0x1FE0,240);
      break;
   default:
        Wbyte(0x1FE0,0xFF);
      break;

    }
    FD1771 = 0;   //clear interrupt flag
    interrupts(); //enable interrupts again
    TIgo();
  }
}
/*void loop() {
 
  //check if flag has set by interrupt routine 
  if (FD1771 == 0xBB) {

    noInterrupts(); //we don't want our interrupt be interrupted

    //if write to CRU Write Register we can go straight back to the TI
    if ( Rbyte(CRUWRI) == lcruw) {

      //nope so prep with reading Command Register
      DSRAM = Rbyte(WCOMND & 0xF0); //keep command only, strip unneeded floppy bits
        
      /*if ( DSRAM != lcmd ) {    //different to last command?
          ccmd = DSRAM;         //yes, set new command
          ncmd = HIGH;          //and set flag
      }
      else {
        ccmd = lcmd;            //no, continue with same command
      }

      //is the selected DSK available?
      cDSK = (Rbyte(CRURD) >> 1) & 0x07;    //determine selected disk
      if ( !aDSK[cDSK] ) {                  //check availability
        Wbyte(RSTAT, Rbyte(RSTAT) & 0x80);  //no; set Not Ready bit in Status Register
        ncmd = LOW;                         //skip new command prep
        ccmd = 208;                         //exit via Force Interrupt command
      }
     
      if ( ccmd < 128 ) { //step/seek commands; no additional prep needed
      
        Wbyte(
        switch (ccmd) {
  	
         	case 0: //restore
            Wbyte(RTRACK,0);  //clear read track register
        		Wbyte(WTRACK,0);  //clear write track register        
        		Wbyte(WDATA,0);   //clear write data register
            sTrack0(0);       //set Track 0 bit in Status Register      
        	break;
    	
        	case 16: //seek; if RTRACK == WDATA direction doesn't change
            if ( Rbyte(RTRACK) > Rbyte(WDATA) ) { 
        		  curdir == LOW;                            //step-in towards track 39
        		}	
        		else if ( Rbyte(RTRACK) < Rbyte(WDATA) ) { 
        		  curdir == HIGH;                           //step-out towards track 0
        		} 
        	  DSRAM = Rbyte(WDATA);                       //read track seek #
        	  Wbyte(RTRACK, DSRAM);                       //update track register
            sTrack0(DSRAM);                             //check, possibly set Track 0 bit in Status Register
          break;
     
    	    case 32: //step
    	      if ( curdir == HIGH ) {         //we could be stepping out towards Track 0 ...
              sTrack0(Rbyte(RTRACK) - 1);   //... so check if we are on Track 1 and if yes set Track 0 bit
    	      }
    		  break;	    
    	
        	case 48: //step+T (update Track register)
        		DSRAM = Rbyte(RTRACK);	//read current track #
        		//is current direction inwards and track # still within limits?
        		if ( curdir == LOW && DSRAM < 39 ) {
        		  Wbyte(RTRACK,++DSRAM);                  //increase track #
        		}
        		//is current direction outwards and track # still within limits?
        		else if ( curdir == HIGH && DSRAM >  0) {
        		  Wbyte(RTRACK,--DSRAM);                  //decrease track #
              sTrack0(DSRAM);                         //check/set Track 0
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
      		  curdir == HIGH;               //set current direction		
            sTrack0(Rbyte(RTRACK) - 1);   //check if we are on Track 1 and if yes set Track 0 bit
          break;
      
          case 112: //step-out+T
            DSRAM = Rbyte(RTRACK);  //read current track #
            //if track # still within limits update track register
            if ( DSRAM > 0) { 
              Wbyte(RTRACK,--DSRAM); 
            }
            curdir == HIGH; //set current direction  
            sTrack0(DSRAM); //check if we are on Track 1 and if yes set Track 0 bit  
          break;
      
        } //end switch seek/step commands
      }
      else { //rest of commands, needing more prep
          
        if ( ncmd ) { //new sector R/W, Track R/F     

          DSK[cDSK].seek(0x10);     //select byte 0x10 in Volume Information Block
          DSRAM = DSK[cDSK].read(); //read that byte
          if ( DSRAM != 32 ) {      //space?
            pDSK = HIGH;            //yes; disk unprotected
          }
          else {
            pDSK = LOW;             //nope, disk is protected
          }

          secval = ( (Rbyte(CRURD) >> 7) * 39) + (Rbyte(RTRACK) * 9) + Rbyte(RSECTR); //calc absolute sector (0-359): (side * 39) + (track * 9) + sector #
          btidx = secval * 256;                                                       //calc absolute DOAD byte index (0-92160)
          DSK[cDSK].seek(btidx);                                                      //set to first absolute byte for R/W
        }

        switch (ccmd) {
           
          case 128: //read sector
            if ( (DSK[cDSK].position() - btidx) < 257 ) { //have we supplied all 256 bytes yet?
  		        DSRAM = DSK[cDSK].read();                 //nope, supply next byte
              Wbyte(RDATA,DSRAM);
  	        }
          break;
		       
          case 144: //read multiple sectors
  	        if ( Rbyte(RSECTR) < 9 ) {      //are we still below the max # of sectors/track? CHECK sector # scheme in source
  	      	  //DSRAM = DSK[cDSK].read();   //yes, supply next byte
             	Wbyte(RDATA, DSK[cDSK].read());
  	        }
  	        Wbyte(RSECTR, Rbyte(RSECTR) + ((DSK[cDSK].position() - btidx) / 256) ); //update Read Sector Register    
	        break;   
	      
          case 160: //write sector
	  	if (!pDSK) {	//only write if DOAD is not protected
		if ( (DSK[cDSK].position() - btidx) < 257 ) { //have we written all 256 bytes yet?
  		        //DSRAM =  DSK[cDSK].read();                 //nope, write next byte
              		DSK[cDSK].write(Rbyte(WDATA) );
  	        }	
		else {
			Wbyte(RSTAT, Rbyte(RSTAT) & 0x80);  //no; set Not Ready bit in Status Register
		}
		
		
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
	        break;
	        case 240: //write track
	        break; 
        }
      }
    }
    //reflect disk select + side select bits in CRU Read register. 
    DSRAM = Rbyte(CRUWRI);
    Wbyte(CRURD,( (DSRAM >> 4) & 0x07) + (DSRAM & 0x80) );
    
    lcmd    = ccmd;               //save current command for compare in next interrupt
    ccmd    = 0;                  //ready to store next command (which could be more of the same)
    lcruw   = Rbyte(CRUWRI);      //save current CRU write register for compare in next interrupt
    lrdi    = Rbyte(RDINT+1);     //save current LSB R6 counter value; next byte in sector R/W, ReadID and track R/F 
    
    FD1771 = 0;   //clear interrupt flag
    interrupts(); //enable interrupts again
  }

} //end of loop()*/

void listen1771() {
  /*//don't want our interrupt to be interrupted
  noInterrupts();
  //copy of TIstop(); faster than calling that function to prevent data corruption
  pinAsOutput(TI_READY);   //switch from HighZ to output
  digitalLow(TI_READY);    //puts TI in wait state and enables 74HC595 shift registers
  digitalHigh(TI_BUFFERS); //disables 74LS541's
  ena_cbus();              //Arduino in control of RAM
  
  //set interrupt flag  
  FD1771=0xBB;*/
}
