/* APEDISK99
  This sketch emulates 3 TI99/4a disk drives. Shopping list:

  - Arduino Uno
	- SD shield (RTC optional but potentially super handy; no guarantees though)
	- APEDSK99 shield (github.com/jambuur/APEDSK99)

  The Arduino RAM interface is based on Mario's EEPROM programmer:
  github.com/mkeller0815/MEEPROMMER

  (much) faster digitalRead / Write by:
  http://masteringarduino.blogspot.com/2013/10/fastest-and-smallest-digitalread-and.html
  usage:
    pinMode( pin, INPUT );        with pinAsInput( pin );
    pinMode( pin, OUTPUT );       with pinAsOutput( pin );
    pinMode( pin, INPUT_PULLUP);  with pinAsInputPullUp( pin );
    digitalWrite( pin, LOW );     with digitalLow( pin );
    digitalWrite( pin, HIGH );    with digitalHigh( pin );
    digitalRead( pin )            with digitalState( pin )
    if( digitalState( pin ) == HIGH )  can be if( isHigh( pin ) )
    digitalState( pin ) == LOW         can be isLow( pin )

  $Author: Jochen Buur $
  $Date: Aug 2019 $
  $Revision: 0.12l $

resetFunc();  This software is freeware and can be modified, re-used or thrown away without any restrictions.

  Use this code at your own risk.
  I'm not responsible for any bad effect or damages caused by this software
*/

//-DSR generic---------------------------------------------------------------------------------- Libraries
//SPI and SD card functions
#include <SPI.h>
#include <SD.h>

//SPI card select pin
#define SPI_CS 10

//-DSR generic--------------------------------------------------------------------------------- Definitions
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

//!!!APEDSK99 board version v0.12m 2020
//74HC595 shift-out definitions
#define CLOCK     17  //PC3
#define LATCH     18  //PC4
#define DS        19  //PC5

//IO lines for Arduino RAM control
#define CE  14  //PC0  LED flashes for both Arduino CE* and TI MBE*
#define WE  16	//PC2

//IO lines for TI99/4a control
#define TI_READY       3  //PD0; TI READY line + enable/disable 74HC595 shift registers
#define TI_INT      	 2  //PD2; 74LS138 interrupt (MBE*, WE* and A15) 
#define TI_BUFFERS    15  //PC1; 74LS541 buffers enable/disable

//error blinking parameters: on, off, delay between sequence
#define LED_ON       500
#define LED_OFF      250
#define LED_REPEAT  1500

//Status Register bits
#define NOTREADY  0x80
#define PROTECTED 0x40
#define NOERROR	  0x00

//"Force Interrupt" command
#define FDINT	0xD0

//"disk" characteristics
#define NRTRACKS   40		//# of tracks/side
#define NRSECTS	    9	  //# sectors/track
#define NRBYSECT  256		//# bytes/sector

//-DSR generic---------------------------------------------------------------------------------- Hardware functions

//short delay function to let bus/signals settle
//doesn't use timers so safe to use in a noInterrupt zone
inline void NOP() __attribute__((always_inline));
void NOP() {
  delayMicroseconds(3);
}

//databus:
//D0,0,PD0 / D1,1,PD1 / D2,4,PD4 / D3,5,PD5 / D4,6,PD6 / D5,7,PD7 / D6,8,PD8 / D7,9,PD9
//switch databus to INPUT state for reading from RAM

void dbus_in() {
  DDRD  &= B00001100;  //set PD7-PD4 and PD1-PD0 to input (D5-D2, D1-D0)
  PORTD &= B00001100;  //initialise input pins
  DDRB  &= B11111100;  //set PB1-PB0 to input (D7-D6)
  PORTB &= B11111100;  //initialise input pins
}

//switch databus to OUTPUT state for writing to RAM
void dbus_out() {
  DDRD  |= B11110011;  //set PD7-PD4 and PD1-PD0 to output (D5-D2, D1-D0)
  DDRB  |= B00000011;  //set PB1-PB0 to output (D7-D6)
}

//disable Arduino control bus; CE* and WE* both HighZ
void dis_cbus() {
  pinAsInput(CE);
  pinAsInput(WE);
}

//enable Arduino control bus; CE* and WE* both HIGH due to 5.1K pull-ups
void ena_cbus() {
  pinAsOutput(CE);
  pinAsOutput(WE);
}

//read a byte from the databus
byte dbus_read()
{
  NOP();				                            //long live the Logic Analyzer
  return ( ((PIND & B00000011)     ) +      //read PD1,PD0 (D1-D0)
           ((PIND & B11110000) >> 2) +      //read PD7-PD3 (D5-D1)     
           ((PINB & B00000011) << 6) );     //read PB1-PBO (D7-D6)
}

//place a byte on the databus
void dbus_write(byte data)
{
  PORTD |= ((data     ) & B00000011);       //write PD1 (D0)
  PORTD |= ((data << 2) & B11110000);       //write PD7-PD3 (D5-D1)
  PORTB |= ((data >> 6) & B00000011);       //write PB1-PBO (D7-D6)
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
  PORTC |= ( (address >>  7) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A12
  digitalLow(CLOCK);
  PORTC |= ( (address >>  6) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A11
  digitalLow(CLOCK);
  PORTC |= ( (address >>  5) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A10
  digitalLow(CLOCK);
  PORTC |= ( (address >>  4) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A9
  digitalLow(CLOCK);
  PORTC |= ( (address >>  3) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A8
  digitalLow(CLOCK);
  PORTC |= ( (address >>  2) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A7
  digitalLow(CLOCK);
  PORTC |= ( (address >>  1) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A6
  digitalLow(CLOCK);
  PORTC |= ( (address      ) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A5
  digitalLow(CLOCK);
  PORTC |= ( (address <<  1) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A4
  digitalLow(CLOCK);
  PORTC |= ( (address <<  2) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A3
  digitalLow(CLOCK);
  PORTC |= ( (address <<  3) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A2
  digitalLow(CLOCK);
  PORTC |= ( (address <<  4) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A1
  digitalLow(CLOCK);
  PORTC |= ( (address <<  5) & B00100000); digitalHigh(CLOCK); digitalLow(DS); //A0

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
  //enable RAM chip select (2x to cater for slower RAM chips)
  digitalLow(CE);
  digitalLow(CE);
  //disable chip select
  digitalHigh(CE);
  //disable write
  digitalHigh(WE);
  //set databus to (default) input state
  dbus_in();
}

//enable TI I/O, disable Arduino shift registers and control bus
inline void TIgo() __attribute__((always_inline));
void TIgo()
{
  dis_cbus();               //cease Arduino RAM control
  pinAsOutput(TI_BUFFERS);  //enable 74LS541's
  EIMSK |= B00000001;       //enable INT0
  pinAsInput(TI_READY);     //switch from output to HighZ: disables 74HC595's and wakes up TI
}

//disable TI I/O, enable Arduino shift registers and control bus
//INLINE: need for speed in ISR
inline void TIstop() __attribute__((always_inline));
void TIstop() {
  EIMSK &= B11111110;       //disable INT0
  pinAsOutput(TI_READY);    //switch from HighZ to output (default LOW)
  pinAsInput(TI_BUFFERS);   //disables 74LS541's
  ena_cbus();               //Arduino in control of RAM
}

//-DSR generic---------------------------------------------------------------------------------------- Hardware Error handling

//flash error code:
//  flash                   : SPI / SD Card fault/not ready
//  flash-flash             : can't read DSR binary image (/APEDSK99.DSR)
//  flash-flash-flash       : no valid DSR header (>AA) at DSR RAM >0000
void eflash(byte error)
{
  //"no APEDSK99 for you" but let user still enjoy a vanilla TI console
  digitalHigh(TI_READY);	    //enable TI but ...
  //... enable Arduino CE* for flashing the error code
  pinAsOutput(CE);
  //error routine: stuck in code flashing loop until reset
  while (true) { 
    for (byte flash = 0; flash < error; flash++)
    {
      //set RAM CE* LOW, turns on LED
      digitalLow(CE);
      //LED is on for a bit
      delay(LED_ON);

      //turn it off
      digitalHigh(CE);
      //LED is off for a bit
      delay(LED_OFF);
    }
    //allow human error interpretation
    delay(LED_REPEAT); 
  }
}

//-APEDSK99 specific-------------------------------------------------------------------------- FD1771 emu: variables and functions

//DOAD file name handling (DSKx 1-3 (2 bytes) + 8 bytes/characters)
#define DTCDSK  0x1FDE
//APEDSK99-specific Command Register (TI BASIC CALL support)
#define ACOMND  0x1FE8
//R6 counter value to detect read access in sector, ReadID and track commands
#define RDINT   0x1FEA
//TI BASIC screen bias
#define TIBias 0x60
//pulling my hair out trouleshooting address
#define aDEBUG 0x1EE0

//CRU emulation bytes + FD1771 registers
#define CRURD   0x1FEC  //emulated 8 CRU input bits           (>5FEC in TI99/4a DSR memory block); not used but possible future use
//B00001111: DSK3 (0x06), DSK2 (0x04), DSK1 (0x02), side 0/1
#define CRUWRI  0x1FEE  //emulated 8 CRU output bits          (>5FEE in TI99/4a DSR memory block)
#define RSTAT   0x1FF0  //read FD1771 Status register         (>5FF0 in TI99/4a DSR memory block)
#define RTRACK  0x1FF2  //read FD1771 Track register          (>5FF2 in TI99/4a DSR memory block)
#define RSECTR  0x1FF4  //read FD1771 Sector register         (>55F4 in TI99/4a DSR memory block)
#define RDATA   0x1FF6  //read FD1771 Data register           (>5FF6 in TI99/4a DSR memory block)
#define WCOMND  0x1FF8  //write FD1771 Command register       (>5FF8 in TI99/4a DSR memory block)
#define WTRACK  0x1FFA  //write FD1771 Track register         (>5FFA in TI99/4a DSR memory block)
#define WSECTR  0x1FFC  //write FD1771 Sector register        (>5FFC in TI99/4a DSR memory block)
#define WDATA   0x1FFE  //write FD1771 Data register          (>5FFE in TI99/4a DSR memory block)

//DSKx file pointers
File DSK[4];  //file pointers to DOAD's

//flags for "drives" (aka DOAD files) available
boolean aDSK[4] = {false, false, false, false};                                                 //disk availability
String  nDSK[4] = {"x", "/DISKS/_DRIVE01.DSK", "/DISKS/_DRIVE02.DSK", "/DISKS/_DRIVE03.DSK"};	  //DOAD file names; startup defaults
boolean pDSK[4] = {false, false, false, false};                                                 //DOAD write protect status aka the adhesive tab
byte    cDSK    = 0;                                                                            //current selected DSK

//various storage and flags for command interpretation and handling
byte DSRAM	              = 0;	    //generic DSR RAM R/W variable
volatile boolean FD1771   = false;  //interrupt routine flag: new or continued FD1771 command
byte FCcmd                = 0;      //current FD1771 command
byte FLcmd                = 0;      //last FD1771 command
boolean FNcmd             = false;  //flag new FD1771 command
byte ACcmd                = 0;      //current APEDSK99 command
unsigned long Dbtidx      = 0;      //absolute DOAD byte index
unsigned long Sbtidx      = 0;	    // R/W sector/byte index counter
byte Ssecidx              = 0;      // R/W sector counter
byte cTrack               = 0;      //current Track #
byte nTrack               = 0;      //new Track # (Seek)
boolean cDir              = HIGH;   //current step direction, step in(wards) towards track 39 by default
String DOAD               = "";	    //TI BASIC CALL support (used by CDSK and SDSK)
char cDot		              = "";	    //"." detection in MSDOS 8.3 format
byte vDEBUG               = 0;      //generic pulling my hair out trouleshooting variable

//no further command execution (prevent seek/step commands to be executed multiple times)
void noExec(void) {
  DSK[cDSK].close();      //close current SD DOAD file
  Wbyte(WCOMND, FDINT);   //"force interrupt" command (aka no further execution)
  FCcmd = FDINT;          //"" ""
  FLcmd = FCcmd;          //reset new FD1771 command prep
  Wbyte(ACOMND, 0x00);    //clear APEDSK99 Command Register
  ACcmd = 0;              //reset APEDSK99 command
  cDSK = 0;               //reset active DSKx
  Dbtidx = 0;             //clear absolute DOAD byte index
  Sbtidx = 0;             //clear byte index counter
  Ssecidx = 0;            //clear sector counter
  cTrack = 0;             //clear current Track #
  nTrack = 0;             //clear new Track #
  DOAD = "";              //clear DOAD name
  cDot = "";              //clear "." DOS extension detection
}

//clear various FD1771 registers (for powerup and Restore command)
void FDrstr(void) {
  Wbyte(RSTAT,  0);       //clear Status Register
  Wbyte(RTRACK, 0);       //clear Read Track register
  //Wbyte(RSECTR, 0x01);	  //default value in Read Sector register
  Wbyte(RSECTR, 0);       //clear Read Sector register
  Wbyte(RDATA,  0);		    //clear Read Data register
  Wbyte(WTRACK, 0);       //clear Write Track register
  //Wbyte(WSECTR, 0x01);	  //default value in Write Sector register
  Wbyte(WSECTR, 0);       //clear Write Sector register
  Wbyte(WDATA,  0);       //clear Write Data register
  Wbyte(RSTAT, NOERROR);	//clear Status register
}

//calculate and return absolute DOAD byte index for R/W commands
unsigned long cDbtidx (void) {
  unsigned long bi;
  bi  = ( Rbyte(CRUWRI) & B00000001 ) * NRTRACKS;     //add side 0 tracks (0x28) if on side 1
  bi += Rbyte(WTRACK);                                //add current track #
  bi *= NRSECTS;                                      //convert to # of sectors
  bi += Rbyte(WSECTR);                                //add current sector #
  bi *= NRBYSECT;                                     //convert to absolute DOAD byte index (max 184320 for DS/SD)
  return (bi);
}

void RWsector( boolean rw ) {
  if ( Sbtidx < NRBYSECT ) {			                    //have we done all 256 bytes yet?
    if ( rw ) {				                                //no; do we need to read a sector?
      Wbyte(RDATA, DSK[cDSK].read() );        	      //yes -> supply next byte
    } else {
      DSK[cDSK].write( Rbyte(WDATA) );        	      //no -> next byte to DOAD
    }
    Sbtidx++;					                                //increase sector byte counter
  } else {
    if (FCcmd == 0x80 || FCcmd == 0xA0) {               //done with R/W single sector
      noExec();                                       //exit
    } else {
      if ( Ssecidx < (NRSECTS - 1) ) {                //last sector (8)?
        Ssecidx++;                                    //no; increase Sector #
        Wbyte(WSECTR, Ssecidx);                       //sync Sector Registers
        Wbyte(RSECTR, Ssecidx);                       //""
        if ( rw ) {                                   //do we need to read a sector?
          Wbyte(RDATA, DSK[cDSK].read() );            //yes -> supply next byte
        } else {
          DSK[cDSK].write( Rbyte(WDATA) );            //no -> next byte to DOAD
        } 
        Sbtidx = 1;                                   //adjust sector byte counter
      } else {
        noExec();                                     //all sectors done; exit
      }
    }
  }
}						                                          //yes; done all 256 bytes in the sector
//----------------------------------------------------------------------------------------------- Setup
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
  //pinAsOutput(TI_BUFFERS);

  //put TI on hold and enable 74HC595 shift registers
  TIstop();

  //--------------------------------------------------------------------------------------------- DSR initialisation
  //read DSR binary from SD and write into DSR RAM

  File InDSR = SD.open("/APEDSK99.DSR", FILE_READ);
  if (InDSR) {
    for ( unsigned int ii = 0; ii < 0x2000; ii++ ) {
      DSRAM = InDSR.read();
      Wbyte(ii, DSRAM);
    }
    InDSR.close();
  } else {
    //couldn't find DSR binary image: flash error 2
    eflash(2);
  }
  //check for valid DSR mark (>AA) at first DSR RAM byte
  if (  Rbyte(0x0000) != 0xAA ) {
    //loading DSR unsuccessful -> flash error 3
    eflash(3);
  }
  
  for ( byte ii = 1; ii < 4; ii++ ) {
    if ( SD.exists(nDSK[ii]) ) {                      //does DOAD x exist?
      aDSK[ii] = true;                                //yes; flag as such
      DSK[ii] = SD.open(nDSK[ii], O_READ);            //open DOAD file to check write protect y/n
      DSK[ii].seek(0x28);                             //byte 0x28 in Volume Information Block stores APEDSK99 adhesive tab status
      pDSK[ii] = ( DSK[ii].read() == 0x50 );          //0x50 || "P" means disk is APEDSK99 "adhesive tab" write protected 
      DSK[ii].close();                                //close current SD DOAD file
    }
  }
 
  //--------------------------------------------------------------------------------------------- Let's go
  //"initialize FD1771":
  FDrstr();   //"Restore" command
  noExec();   //"no command" as default

  //enable TI interrupts (MBE*, WE* and A15 -> 74LS138 O0)
  //direct interrupt register access for speed (attachInterrupt is too slow)
  //EICRA |= (1 << ISC00);  //sense any change on the INT0 pin
  EICRA &= B11110000;       //sense LOW on the INT0 pin
  //EIMSK |= (1 << INT0);     //enable INT0 interrupt

  //TI: take it away
  TIgo();

} //end of setup()

//------------------------------------------------------------------------------------------------ Loop
void loop() {

 
  //check if flag has set by interrupt routine
  if (FD1771) {

    //read Command Register, stripping the unnecessary floppy bits but keeping command nybble
    FCcmd = Rbyte(WCOMND) & B11110000;

    //the FD1771 "Force Interrupt" command is used to stop further command execution
    if ( FCcmd != FDINT ) { //do we need to do anything?
      
      FNcmd = (FCcmd != FLcmd);                             //new or continue previous FD1771 command?
      if (FNcmd) {                                          //new command?
        FLcmd = FCcmd;                                      //yes; remember new command for next compare
      }
      //----------------------------------------------------------------------------------------- FD1771 Seek / Step
      if ( FCcmd < 0x80 ) {                                 //step/seek commands?

        cTrack = Rbyte(WTRACK);                             //read current Track #
        nTrack = Rbyte(WDATA);                              //read new Track # (Seek)

        switch(FCcmd) {
         
          case 0x00:					                              //Restore            
          { 
	          /* cTrack = 0;                                     //reset cTrack so Track Registers will be cleared after switch{}
            FDrstr(); */
            Wbyte(aDEBUG, vDEBUG);
            if ( vDEBUG == 0xFF ) {
              vDEBUG = 0;
            } else {
              vDEBUG++;              
            }
          }  
          break;

          case 0x10:					                              //Seek
          {
            cTrack = nTrack;                                //prepare to update Track Register
	          cDir = (nTrack > cTrack);                       //set direction: HIGH=track # increase, LOW=track # decrease
          } 
          break;
            
          case 0x20:                                        //Step
          case 0x30:                                        //Step+T
          {
            if ( cDir == LOW ) {
              FCcmd = 0x70;				                          //execute Step-Out+T
            } else {
              FCcmd = 0x50;				                          //execute Step-In+T
	          }
          }
          case 0x40:                                        //Step-In
          case 0x50:                                        //Step-In+T
          {
            if ( cTrack < NRTRACKS ) {			                //any tracks left to step to?
              cTrack++;                                     //yes; increase Track #
              cDir = HIGH;                                  //set stepping direction towards last track 
            }
          }
          break;

          case 0x60:                                        //Step-Out
          case 0x70:                                        //Step-Out+T
          {
            if ( cTrack > 0 ) {				                      //any tracks left to step to?
              cTrack--;                                     //decrease Track #
              cDir = LOW;                                   //set stepping direction towards track 0
            }
          }
          break;
            
        } //end switch FCcmd
        Wbyte(WTRACK, cTrack);				                      //update Track Register
	      Wbyte(RTRACK, cTrack);                              //sync Track Registers
        noExec();                                           //prevent multiple step/seek execution                                        

      // end FCcmd < 0x80
      } else {  // read/write commands
        
        //----------------------------------------------------------------------------------------- FD1771 R/W commands: prep
        if ( FNcmd ) {					                                  //new command prep
          cDSK = ( Rbyte(CRUWRI) >> 1) & B00000011;             //yes do some prep; determine selected disk
          if ( aDSK[cDSK] ) {                                   //is selected disk available?
            Wbyte(RSTAT, NOERROR);                              //reset "Not Ready" bit in Status Register
            if ( FCcmd == 0xE0 || FCcmd == 0xF0 ) {               // R/W whole track?
              Wbyte(WSECTR, 0x00);                              //yes; start from sector 0
            }
            DSRAM = Rbyte(WSECTR);                              //store starting sector #
            DSK[cDSK] = SD.open(nDSK[cDSK], O_READ | O_WRITE);  //open SD DOAD file
            Dbtidx = cDbtidx();                                 //calc absolute DOAD byte index
            DSK[cDSK].seek(Dbtidx);                             //set to first absolute DOAD byte for R/W
          } else {
            if ( cDSK != 0 ) {                                  //ignore DSK0; either DSK1, DSK2 or DSK3 is not available
              Wbyte(RSTAT, NOTREADY);                           //set "Not Ready" bit in Status Register
              FCcmd = FDINT;                                    //exit 
            }
          }
        }
        
        //----------------------------------------------------------------------------------------- FD1771 R/W commands
        switch (FCcmd) {  //switch R/W commands

          case 0xD0:
          {
            noExec();
          }
          break;

          case 0x80:                                      //read sector
	        case 0x90:                                      //read multiple sectors
	        case 0xE0:                                      //read track		
          {
            RWsector( true );
          }
          break;

          case 0xA0:         				                      //write sector
	        case 0xB0:                                      //write multiple sectors
          case 0xF0:                                      //write track
          {
            if ( !pDSK[cDSK] ) {                          //is DOAD write protected?
              RWsector( false );                          //no; go ahead and write
            } else {
              Wbyte(RSTAT, PROTECTED);                    //yes; set "Write Protect" bit in Status Register
              FCcmd = FDINT;                              //exit      
            }
          }
          break;

        } //end R/W switch
      } //end else R/W commands

    //end we needed to do something
    } else {    

      //------------------------------------------------------------------------------------------- APEDSK99-specifc commands
      //check for APEDSK99-specfic commands
      ACcmd = Rbyte(ACOMND);
      if ( ACcmd != 0 ) {
    
        //----------------------------------------------------------------- TI BASIC PDSK(), UDSK(), CDSK(), SDSK() and FDSK()
        switch ( ACcmd ) {

          case  1:                                                            //UDSK(1):  Unprotect DSK1
          case  2:                                                            //UDSK(2):  Unprotect DSK2
          case  3:                                                            //UDSK(3):  Unprotect DSK3      
          case  5:                                                            //PDSK(1):  Protect DSK1
          case  6:                                                            //PDSK(2):  Protect DSK2
          case  7:                                                            //PDSK(3):  Protect DSK3
          {
            cDSK = ACcmd & B00000011;                                         //strip U/P flag, keep DSKx
            if ( aDSK[cDSK] ) {
              
              DSK[cDSK] = SD.open(nDSK[cDSK], O_READ | O_WRITE);              //open DOAD file to change write protect status 
              DSK[cDSK].seek(0x28);                                           //byte 0x28 in Volume Information Block stores APEDSK99 adhesive tab status 
              if ( ACcmd & B00000100 ) {                                      //Protect bit set?
                DSK[cDSK].write(0x50);                                        //yes; apply adhesive tab
                pDSK[cDSK] = true;
              } else {
                DSK[cDSK].write(0x20);                                        //no; remove adhesive tab
                pDSK[cDSK] = false;            
              }
            } 
            noExec();
          }
          break;  

          case 9:                                                             //CDSK(): Change DOAD mapping
 	        {
            for ( byte ii = 2; ii < 10; ii++ ) {                              //merge CALL CDSK characters into string
              DOAD += char( Rbyte(DTCDSK + ii) );
            }          
            DOAD.trim();                                                      //remove leading / trailing spaces
            DOAD = "/DISKS/" + DOAD + ".DSK";                                 //construct full DOAD path
            
            if ( SD.exists( DOAD ) ) {                                        //exists?
              cDSK = Rbyte(DTCDSK);                                           //yes; assign to requested DSKx
              nDSK[cDSK] = DOAD;
              aDSK[cDSK] = true;
              DSK[cDSK] = SD.open(nDSK[cDSK], FILE_READ);                     //open new DOAD file to check write protect y/n
              DSK[cDSK].seek(0x28);                                           //byte 0x28 in Volume Information Block stores APEDSK99 adhesive tab status
              pDSK[cDSK] = ( DSK[cDSK].read() == 0x50 );                      //0x50 || "P" means disk is write APEDSK99 protected
              DSK[cDSK].close();                                              //close new DOAD file
            } else {
              Wbyte(DTCDSK, 0xFF);                                            //no; return error flag
	          }    
	          noExec();
 	        } 
	        break;        
            
          case 10:                                                            //SDSK(): Show DOAD mapping       
		      {
		        cDSK = Rbyte(DTCDSK);						                                  //is the requested disk mapped to a DOAD?
	          if ( aDSK[cDSK] ) {
		         DOAD = nDSK[cDSK];						                                    //yes; get current DOAD name
		        } else {
		          DOAD = "/DISKS/<NO MAP>";					                              //no; indicate as such
		        }
        		Wbyte(DTCDSK    , cDSK+48+TIBias);				                        //drive # in ASCII + TI BASIC bias
        		Wbyte(DTCDSK + 1, '=' +   TIBias);		  	                        //"=" in ASCII + TI BASIC bias
        		for ( byte ii = 2; ii < 10; ii++ ) {
		          cDot = DOAD.charAt(ii+5) + TIBias;                              //read character and add TI BASIC bias  	  
		          if ( cDot != char(142) ) {                                      //is it "."?
                Wbyte(DTCDSK + ii, cDot);                                     //no; prepare next character
              } else {
                ii = 11;                                                      //yes; don't print, end loop          
              }	   
        		}
 	          noExec();
		      } 
 	        break;       
   
        } //end switch accmd commands   
      } //end check APEDSK99-specific commands                                 
    } //end else 

    //----------------------------------------------------------------------------------------------- End of command processing, wait for next interrupt (TI write to DSR space)
    FD1771 = false;   //clear interrupt flag
    
    TIgo(); 

  } //end FD1771 flag check

} //end loop()

//---------------------------------------------------------------------------------------------------- Interrupt routine; puts TI on hold and sets flag
//Interrupt Service Routine (INT0 on pin 2)
ISR(INT0_vect) {

  TIstop();

  //set interrupt flag
  FD1771 = true;
}
