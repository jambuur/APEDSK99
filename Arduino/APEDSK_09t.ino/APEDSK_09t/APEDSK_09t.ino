/* APEDISK99
* CHECK: WTRACK/WSECTR update, READ ID byte counter
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
#define CRURD   0x1FEC  //emulated 8 CRU input bits      (>5FEC in TI99/4a DSR memory block); not used but possible future use
//B00001111: DSK1, DSK2, DSK3, side 0/1
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
#define LED_on      500
#define LED_off     250
#define LED_repeat  1500

//useful Status Register bits
#define NotReady 0x80
#define Protect  0x40
#define Track0   0x04

//"disk" characteristics
#define maxtrack  0x28		//# of tracks
#define maxsect	  0x09		//# of sectors/track
#define maxbyte   0x100		//# of bytes per sector

//short delay function to let bus/signals settle
//doesn't use timers so safe to use in a noInterrupt zone
inline void NOP() __attribute__((always_inline));
void NOP() {
  delayMicroseconds(3);
}  

//switch databus to INPUT state for reading from RAM
void dbus_in() {
  DDRD  &= B00000101;  //set PD7-PD3 and PD1 to input (D5-D1, D0) 
  PORTD &= B00000101;  //initialise input pins
  DDRB  &= B11111100;  //set PB1, PB0 to input (D7, D6)
  PORTB &= B11111100;  //initialise input pins
}

//switch databus to OUTPUT state for writing to RAM
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
  pinAsOutput(CE);
  digitalHigh(CE);   	//default output state is LOW, we obviously don't want that
  pinAsOutput(WE);
  digitalHigh(WE);  	//default output state is LOW, we obviously don't want that
}

//read a byte from the databus
byte dbus_read() 
{   
  NOP();				                        //long live the Logic Analyzer
  return( ((PIND & B00000010) >> 1) +   //read PD1 (D0)
          ((PIND & B11111000) >> 2) +   //read PD7-PD3 (D5-D1)
          ((PINB & B00000011) << 6) );  //read PB1, PBO (D7, D6) 
 } 

//place a byte on the databus
void dbus_write(byte data)
{
  NOP();
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
  //set databus to (default) input state
  dbus_in();
}

//disable TI I/O, enable Arduino shift registers and control bus
//INLINE: need for speed in ISR
inline void TIstop() __attribute__((always_inline));
void TIstop() {
   NOP();                   //long live the Logic Analyzer
   pinAsOutput(TI_READY);   //switch from HighZ to output
   digitalHigh(TI_BUFFERS); //disables 74LS541's
   ena_cbus();              //Arduino in RAM control
}

//enable TI I/O, disable Arduino shift registers and control bus
void TIgo()
{
  dis_cbus();               //cease Arduino RAM control
  digitalLow(TI_BUFFERS);   //enable 74LS541's 
  pinAsInput(TI_READY);     //switch from output to HighZ: disables 74HC595's and wakes up TI
}

//flash error code:
//  flash                   : SPI / SD Card fault/not ready
//  flash-flash             : can't read DSR binary image (/APEDSK.DSR) 
//  flash-flash-flash       : no valid DSR header (>AA) at DSR RAM >0000
//  flash-flash-flash-flash : can't read default DSK1 image (/001.DSK)
void eflash(byte error)
{
  //"no APEDSK99 for you" but let user still enjoy a vanilla TI console
  digitalHigh(TI_BUFFERS);	//disable 74LS541's
  digitalHigh(TI_READY);	//enable TI but ...
  //... enable Arduino CE* for flashing the error code
  pinAsOutput(CE);

  //error routine: stuck in code flashing loop until reset
  while (true) {

    for (byte flash = 0; flash < error; flash++)
    {
      //set RAM CE* LOW, turns on LED
      digitalLow(CE);  
      //LED is on for a bit
      delay(LED_on);

      //turn it off
      digitalHigh(CE); 
      //LED is off for a bit
      delay(LED_off);
    } 
      //allow human error interpretation
      delay(LED_repeat);
  }
}

//set Status Register bit(s):
//B11100100: NotReady, Protect, Head Loaded and Track0
void sStatus (byte sbit, boolean set) {
  if ( set ) {
    Wbyte(RSTAT, (Rbyte(RSTAT) | sbit) );
  }
  else {
    Wbyte(RSTAT, (Rbyte(RSTAT) & ~sbit) );
  }
}

//DSR binary input file pointer
File InDSR;  

//DSKx file pointers
File DSK[4];  //file pointers to DOAD's

//flags for "drives" (aka DOAD files) available (DSK1 should always be available, if not Error 4)
boolean aDSK[4] = {false,true,false,false};                                         //disk availability
String  nDSK[4] = {"x","/DISKS/001.DSK","/DISKS/002.DSK","/DISKS/003.DSK"};	        //DOAD file name
byte    cDSK    = 0;                                                                //current selected DSK
boolean pDSK    = false;                                                            //protected DSK flag

//various storage and flags for command interpretation and handling
byte DSRAM	            = 0;	    //generic variable for RAM R/W
volatile boolean FD1771 = false;  //interrupt routine flag: new or continued FD1771 command
byte ccmd               = 0;      //current command
byte lcmd               = 0;      //last command
boolean ncmd            = false;  //flag new command
unsigned long int btidx = 0;      //absolute DOAD byte index: (secval * 256) + repeat R/W
boolean curdir          = LOW;    //current step direction, step in(wards) towards track 39 by default
unsigned int sectidx    = 0;	    //R/W and READ ID index counter 

//clear various FD1771 registers (for powerup and Restore command)
void FDrstr(void) {
  Wbyte(RTRACK,0);        //clear Read Track register
  Wbyte(RSECTR,0);	      //clear Read Sector register
  Wbyte(RDATA, 0);		    //clear Read Data register
  Wbyte(WTRACK,0);        //clear Write Track register  
  Wbyte(WSECTR,0);	      //clear Write Sector register
  Wbyte(WDATA,0);         //clear Write Data register
  sStatus(Track0, true);  //set Track 0 bit in Status Register
}

//no further command execution (prevent seek/step commands to be executed multiple times)
void noExec(void) {
  DSK[cDSK].close();  //close current SD DOAD file
  Wbyte(WCOMND,0xD0); //"force interrupt" command (aka no further execution)
  ccmd = 0xD0;        // "" ""
  lcmd = ccmd;		    //reset new command prep
  sectidx = 0; 	      //clear index counter
}

//calculate and return absolute DOAD byte index for R/W commands 
unsigned long int cbtidx (void) {
  unsigned long int bi;
  bi  = ( Rbyte(CRUWRI) & B00000001 ) * (maxtrack);   //add side 0 tracks (0x28) if on side 1
  bi += Rbyte(WTRACK);                                //add current track #
  bi *= 9;                                            //convert to # of sectors
  bi += Rbyte(WSECTR);                                //add current sector #
  bi *= 256;                                          //convert to absolute DOAD byte index (max 184320 for DS/SD)
  return (bi);
}

unsigned int debug = 0xA000;

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
  else {
    //couldn't find DSR binary image: flash error 2
    eflash(2);
  }
  //check for valid DSR mark (>AA) at first DSR RAM byte
  if (  Rbyte(0x0000) != 0xAA ) {
    //loading DSR unsuccessful -> flash error 3
    eflash(3);
  }

  //try to open DSK1 to see if it's available (it should)
  DSK[1] = SD.open(nDSK[1], FILE_READ);
  if ( !DSK[1] ) {
    eflash(4);	//could not open DSK1 -> flash error 4
  }
  else {
    DSK[1].close();
  }
    
  //try to open DSK2 and if yes set flag (default is false)
  DSK[2] = SD.open(nDSK[2], FILE_READ);
  if ( DSK[2] ) {
    //close file and set flag
    DSK[2].close();
    aDSK[2] = true;
  }
  
  //try to open DSK3 and if yes set flag (default is false)
  DSK[3] = SD.open(nDSK[3], FILE_READ);
  if ( DSK[3] ) {
    //close file and set flag
    DSK[3].close();
    aDSK[3] = true;
  }
     
  //initialize FD1771:
  // - initialise Status Register: "Head Loaded" set
  Wbyte(RSTAT, 0x20); 
  // - "Restore" command
  FDrstr();
  // - "no command" as default
  noExec();

  //enable TI interrupts (MBE*, WE* and A15 -> 74LS138 O0)
  //direct interrupt register access for speed (attachInterrupt is too slow)
  EICRA = 1 << ISC00;  //sense any change on the INT0 pin
  EIMSK = 1 << INT0;   //enable INT0 interrupt
	
  //TI: take it away
  TIgo();  

} //end of setup()

void loop() {

  //check if flag has set by interrupt routine 
  if (FD1771) {
    
    //disable interrupts; although the TI is on hold and won't generate interrupts, the Arduino is ready to rumble
    noInterrupts();

    //read Command Register, stripping the unnecessary floppy bits
    ccmd = Rbyte(WCOMND) & 0xF0;

    //the FD1771 "Force Interrupt" command is used to flag further command execution is not needed
    if ( ccmd != 0xD0 ) { //do we need to do anything?

      //new or continue previous command?
      ncmd = (ccmd != lcmd);
      if (ncmd) {                                       //new command?
        
        //is the selected DSK available?
        cDSK = (Rbyte(CRUWRI) >> 1) & B00000011;        //yes; determine selected disk
        if ( aDSK[cDSK] ) {                             //is selected disk available?

          lcmd = ccmd;                                  //yes; remember new command for next compare
          
          sStatus(NotReady,false);                      //reset "Not Ready" bit in Status Register  
          DSK[cDSK] = SD.open(nDSK[cDSK], FILE_WRITE);  //open SD DOAD file        
          DSK[cDSK].seek(0x10);                         //byte 0x10 in Volume Information Block stores Protected flag
          pDSK = DSK[cDSK].read() != 0x20;              //disk is protected when byte 0x10 <> " "
          sStatus(Protect,pDSK);                        //reflect "Protect" status 
          btidx = cbtidx();                             //calc absolute DOAD byte index
          DSK[cDSK].seek(btidx);                        //set to first absolute byte for R/W  
          sectidx = 0;                                  //clear sector index counter   
        }
        else {      
          sStatus(NotReady,true);                       //no; set "Not Ready" bit in Status Register
          noExec();                                     //prevent multiple step/seek execution
        }   
      }

      if ( ccmd < 0x80 ) { //step/seek commands?
      
        switch (ccmd) { //yep switch step/seek commands

          case 0x00:	//restore
            FDrstr();
          break;
			
          case 0x10:	//seek
            DSRAM = Rbyte(WDATA);             //read track seek #
            if ( DSRAM <= maxtrack) {         //make sure we stay within max # of tracks
              if ( DSRAM > Rbyte(WTRACK) ) { 
                curdir = LOW;                 //step-in towards track 39
                sStatus(Track0, false);       //reset Track 0 bit in Status Register
              } 
              else {
                if ( DSRAM < Rbyte(WTRACK) ) { 
                  curdir = HIGH;              //step-out towards track 0
                  sStatus(Track0, DSRAM==0);  //check for Track 0 and set Status Register accordingly
                }  
              }
	            Wbyte(WTRACK, DSRAM);           //update track register
            }
	        break;

          case 0x20:	//step
            //always execute step+T, can't see it making any difference (FLW)  
    
          case 0x30:	                              //step+T (update Track register)
            DSRAM = Rbyte(WTRACK);                  //read current track #
            //is current direction inwards and track # still within limits?
            if (  DSRAM < 39  && curdir == LOW ) {
              Wbyte(WTRACK,++DSRAM);                //increase track #
              sStatus(Track0, false);               //reset Track 0 bit in Status Register
            }
            else {
              //is current direction outwards and track # still within limits?
              if ( DSRAM >  0 && curdir == HIGH ) {
                Wbyte(WTRACK,--DSRAM);  	          //decrease track #
                sStatus(Track0, DSRAM==0);          //check for Track 0 and set Status Register accordingly
              }
            }      
          break;
                   
          case 0x40:	//step-in (towards track 39)
            //always execute step-in+T, can't see it making any difference (FLW)
         
          case 0x50:	//step-in+T (towards track 39, update Track Register)
            DSRAM = Rbyte(WTRACK);      //read current track #
            //if track # still within limits update track register
            if ( DSRAM < maxtrack) { 
              Wbyte(WTRACK,++DSRAM);    //increase track #
              curdir = LOW; 	          //set current direction    
              sStatus(Track0, false);   //reset Track 0 bit in Status Register
      	    }
           break;
          
          case 0x60:	//step-out (towards track 0)
            //always execute step-out+T, can't see it making any difference (FLW)
 
          case 0x70:	//step-out+T
            DSRAM = Rbyte(WTRACK);      //read current track #
            //if track # still within limits update track register
            if ( DSRAM > 0) { 
              Wbyte(WTRACK,--DSRAM);    //decrease track #
            }
            curdir = HIGH; //set current direction  
            sStatus(Track0, DSRAM==0);  //check for Track 0 and Status Register accordingly
          break;
        
        } //end switch step commands
       
	      Wbyte(RTRACK, Rbyte(WTRACK) );  //sync Track Registers
        noExec();                       //prevent multiple step/seek execution 

      } // end ccmd < 0x80
    
      else {  // read/write commands
        
        switch (ccmd) {  //switch R/W commands

          case 0xD0:
            noExec(); 
          break;
          
	        case 0xE0: //read entire track; which sounds like reading multiple sectors (fallthrough to 0x90: read multiple sectors)		
			
          case 0x90: //read multiple sectors; which sounds like reading single sectors in a loop (fallthrough to 0x80: read sector)
        
          case 0x80: //read sector
            sectidx++;                                                                                            //increase byte index                 
            if ( sectidx <= maxbyte) {                                                                            //have we supplied all 256 bytes yet?  
              Wbyte(RDATA, DSK[cDSK].read() );                                                                    //nope, supply next byte
            }
            else {
              DSRAM = Rbyte(WSECTR);
              Wbyte(WSECTR, ++DSRAM );                                    			                                  //increase Sector Register
              if ( (ccmd == 0xE0 || ccmd == 0x90) && (DSK[cDSK].position() - btidx) <= (maxsect * maxbyte) ) {   	//multi-read: did we get all sectors in the track?
              	sectidx = 0;                                         				                                      //not all 2304 bytes/9 sectors supplied yet -> next one
              }
	            else {
                 Wbyte(RSECTR, Rbyte(WSECTR) );                                                                   //sync Sector Registers
	              noExec();                                                   			                                //we're done; exit via Force Interrupt command
	            }
	          }
          break;     
          
          case 0xF0: //write entire track; which sounds like writing multiple sectors (fallthrough to 0xB0: write multiple sectors)
		    
          case 0xB0: //write multiple sectors; which sounds like writing single sectors in a loop (fallthrough to 0xA0: write sector)
        
          case 0xA0: //write sector
            if ( !pDSK ) {    
	            sectidx++;                                                                                          //increase byte index                 
              if ( sectidx <= maxbyte) {                                                                          //have we written all 256 bytes yet?  
                DSK[cDSK].write( Rbyte(RDATA) );			                                                            //nope, write next byte                            
              }
              else {
                DSRAM = Rbyte(WSECTR);
                Wbyte( WSECTR, ++DSRAM );                                    			                                //increase Sector Register
                if ( (ccmd == 0xF0 || ccmd == 0xB0) && (DSK[cDSK].position() - btidx) <= (maxsect * maxbyte) ) {  //multi-write: did we write all sectors in the track?
              	  sectidx = 0;                                         				                                    //not all 2304 bytes/9 sectors supplied yet -> next one
	              }
	              else {
	                Wbyte(RSECTR, Rbyte(WSECTR) );                                                                  //sync Sector Registers
	                noExec();                                                   			                              //we're done; exit via Force Interrupt command
		            }
	            }
            }
	          else {
	            //sStatus(NotReady, 1);			                                                                        //trying to write to a protected disk (DEBUG: verify behaviour)
	            noExec();                                                                                         //we're done; exit via Force Interrupt command
	          }
          break;   	    

      	  case 0xC0:  //read ID
      	    sectidx++;                                    //increase byte index
      	    switch (sectidx) {
      	      case 1:
      	        Wbyte(RDATA, Rbyte(RTRACK) );		          //track #
      	      break;
      	      case 2:
      	        Wbyte(RDATA, Rbyte(CRUWRI) & B00000001);  //side #
      	      break;
      	      case 3:
                Wbyte(RDATA, Rbyte(RSECTR) );		          //sector #
                break;
      	      case 4:
                Wbyte(RDATA, 0x01); 		                  //sector size (256 bytes)
              break;
      	      case 5:
      		      Wbyte(RDATA, 0x0C);	                      //CC bogus byte #1
              break;
      	      case 6:
                Wbyte(RDATA, 0x0D);	                      //CC bogus byte #2
      		      noExec();		                              //and we're done with READ ID
      	      break;
      	    } //end switch READ ID
	     	  
	     	  break;
        } //end switch non-step commands      
		    
      } //end R/W commands

    } //end we needed to do something

    FD1771 = false;   //clear interrupt flag

    interrupts();     //enable interrupts for the next round  
    
    TIgo();

  } //end FD1771 flag check

} //end loop()
		
//Interrupt Service Routine (INT0 on pin 2)
ISR(INT0_vect) { 

  TIstop();
   
  //set interrupt flag  
  FD1771=true;  
  
}
