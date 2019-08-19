/**
 * ** project Arduino EEPROM programmer **
 * 
 * This sketch can be used to read and write data to a
 * AT28C64 or AT28C256 parallel EEPROM
 *
 * $Author: mario $
 * $Date: 2013/05/05 11:01:54 $
 * $Revision: 1.2 $
 *
 * This software is freeware and can be modified, reused or thrown away without any restrictions.
 *
 * Use this code at your own risk. I'm not responsible for any bad effect or damages caused by this software!!!
 *
 **/

//SPI and SD card functions
#include <SPI.h>
#include <SD.h>

//SPI card select pin
#define SPI_CS 10

//74HC595 shift-out definitions
#define DS      A3
#define LATCH   A4
#define CLOCK   A5

#define PORTC_DS      3
#define PORTC_LATCH   4
#define PORTC_CLOCK   5

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
#define CE A0 //LED lights up for both Arduino CE* and TI MBE*
#define WE A2
#define PORTC_CE   0
#define PORTC_WE   2

//define IO lines for Arduino -> TI99/4a control
#define TI_BUFFERS A1  //74LS244 buffers enable/disable
#define TI_READY 0     //TI READY line; also enables/disables 74HC595 shift registers
#define TI_INT 2       //74LS138 interrupt (MBE*, WE* and A15) 

//flag from interrupt routine to signal possible new FD1771 command
volatile byte cmd_FD1771 = 0;

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
byte wcomnd = 0;
#define WTRACK  0x1FFA     //write FD1771 Track register
boolean curdir = LOW;
#define WSECTR  0x1FFC     //write FD1771 Sector register
#define WDATA   0x1FFE     //write FD1771 Data register

//sector buffer for TI disk file operations
#define BUFFERSIZE 256
byte sector[BUFFERSIZE];

//error blinking parameters: on, off, delay between sequence
#define LED_on  350
#define LED_off 250
#define error_repeat 1500

// input and output file pointers
File InDSR;
File InDSK1;
File OutDSK1;
File InDSK2;
File OutDSK2;
File InDSK3;
File OutDSK3;

boolean DSK2_present = LOW;
boolean DSK3_present = LOW;

/*****************************************************************
 *
 *  CONTROL and DATA functions
 *
 ****************************************************************/

// switch databus to INPUT state
void databus_input() {
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
void databus_output() {
  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);
}

//switch control bus to INPUT state
void cbus_input() {
  pinMode(CE, INPUT);
  pinMode(WE, INPUT);
}
 
//switch control bus to OUTPUT state
void cbus_output() {
  pinMode(CE, OUTPUT);
  pinMode(WE, OUTPUT);
}

//read a complete byte from the databus
//!be sure to set databus to input first
byte read_databus()
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
//!be sure to set databus to output first 
void write_databus(byte data)
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

//shift out the given address to the 74hc595 registers
void set_addressbus(unsigned int address)
{
  //get MSB of 16 bit address
  byte hi = address >> 8;
  //get LSB of 16 bit address
  byte low = address & 0xff;

  //disable latch line
  bitClear(PORTC,PORTC_LATCH);

  //shift out MSB byte
  fastShiftOut(hi);
  //shift out LSB byte
  fastShiftOut(low);

  //enable latch and set address
  bitSet(PORTC,PORTC_LATCH);
}

//faster shiftOut function then normal IDE function (about 4 times)
void fastShiftOut(byte data) {
  //clear data pin
  bitClear(PORTC,PORTC_DS);
  //Send each bit of the myDataOut byte; MSB first
  for (int i=7; i>=0; i--)  {
    bitClear(PORTC,PORTC_CLOCK);
    //Turn data on or off based on value of bit
    if ( bitRead(data,i) == 1) {
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

//short function to set RAM CE* (other 2 RAM enable/enable lines (OE* and CE) are permanenty enabled)
void set_ce (byte state)
{
  digitalWrite(CE, state);
}

//short function to set RAM WE* (Write Enable)
void set_we (byte state)
{
  digitalWrite(WE, state);
}

//short function to enable/disable TI interface 74LS244 buffers
void set_tibuffers (byte state)
{
  digitalWrite(TI_BUFFERS, state);
}

//short function to set TI READY line (pause TI); 74HC595 buffers are set the opposite at the same time
void set_tiready (byte state)
{
  digitalWrite(TI_READY, state);
}

//short function to disable TI I/O
void set_tistop()
{
  set_tibuffers(HIGH);
  delayMicroseconds(5);
  set_tiready(LOW);
  delayMicroseconds(5);
}

//short function to enable TI I/O
void set_tigo()
{
  set_tibuffers(LOW);
  delayMicroseconds(5);
  set_tiready(HIGH);
  delayMicroseconds(5);
}

//highlevel function to read a byte from a given address
byte read_byte(unsigned int address)
{
  byte data = 0;
 
  //disable RAM write
  set_we(HIGH);
  //set address bus
  set_addressbus(address);
  //set databus for reading
  databus_input();
  //enable RAM chip select
  set_ce(LOW);
  //get databus
  data = read_databus();
  //disable RAM chip select
  set_ce(HIGH);
 
  return data;
}

//highlevel function to write a byte to a given address
void fast_write(unsigned int address, byte data)
{
  //set address bus
  set_addressbus(address);
  //set databus for writing
  databus_output();
  //enable chip select
  set_ce(LOW);
  //enable write
  set_we(LOW);
  //set data bus
  write_databus(data);
  //disable write
  set_we(HIGH);
  //disable chip select
  set_ce(HIGH);
  //set databus to input
  databus_input();
}

//flash error code
void flash_error(byte error)
{
  //get into working TI state ...
  cbus_input();
  set_tigo();
  //... but disable DSR I/O for TI ...
  set_tibuffers(HIGH);
  //... and make sure we can toggle Arduino CE*
  pinMode(CE, OUTPUT);

  //stuck in error flashing loop until reset
  while (1) {

    for (byte flash =0; flash < error; flash++)
    {
      //enable RAM for Arduino, turns on LED
      set_ce(LOW);
      //LED is on for a bit
      delay(LED_on);
      //disable RAM for Arduino, turns off LED
      set_ce(HIGH);
      //LED is off for a bit
      delay(LED_off);
    } 
    delay(error_repeat);
  }
}
  
 /************************************************
 *
 * INPUT / OUTPUT Functions
 *
 *************************************************/

/*
 * write a data block to the eeprom
 * @param address  startaddress to write on eeprom
 * @param buffer   data buffer to get the data from
 * @param len      number of bytes to be written
 **/
void write_block(unsigned int address, byte* buffer, int len) {
  for (unsigned int i = 0; i < len; i++) {
    fast_write(address+i,buffer[i]);
  }   
}

/************************************************
 *
 * MAIN
 *
 *************************************************/
void setup() {

  //74HC595 shift register control
  pinMode(DS, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode(CLOCK, OUTPUT);

  //TI99/4a I/O control
  pinMode(TI_READY, OUTPUT);
  pinMode(TI_BUFFERS, OUTPUT);
  //74LS138 interrupt: write to DSR space (aka FD1771 registers)
  pinMode(TI_INT, INPUT);

  //see if the SD card is present and can be initialized
  if (!SD.begin(SPI_CS)) {
    //nope -> flash LED error 1
    flash_error(1);
  }

  //put TI on hold and enable 74HC595 shift registers
  set_tistop();
  //enable Arduino control bus signals (CE* and WE*)
  cbus_output();
  
  //check for existing DSR: read first DSR RAM byte (>4000 in TI address space) ...
  byte DSRAM = read_byte(0x0000);
  
  // ... and check for valid DSR header (>AA)
  if ( DSRAM != 0xAA ) {
    //didn't find header so read DSR binary from SD and write into DSR RAM
    InDSR = SD.open("/APEDSK.DSR", FILE_READ);
    if (InDSR) {
      for (int CByte =0; CByte < 0x2000; CByte++) {
        DSRAM = InDSR.read();
        fast_write(CByte,DSRAM);
      }
    InDSR.close();
    }
    else {
      //couldn't open SD DSR file -> flash LED error 2
      flash_error(2);
    }
  }

  //open DSK1 (error if doesn't exist)
  InDSK1 = SD.open("/DISKS/001.DSK", FILE_READ);
    if (!InDSK1) {
      flash_error(3);
    }
  //try to open DSK2 and flag if exists
  InDSK2 = SD.open("/DISKS/002.DSK", FILE_READ);
    if (InDSK2) {
      DSK2_present = HIGH;
    }
  //try top open DSK3 and flag if exists
  InDSK3 = SD.open("/DISKS/003.DSK", FILE_READ);  
    if (InDSK3) {
      DSK3_present = HIGH;
    }

  //initialise FD1771 (clear all CRU and FD1771 registers)
  for (int CByte = CRURD; CByte <= WDATA+2; CByte++) {
    fast_write(CByte,0);
  }
  /*
  //disable Arduino control bus signals (CE* and WE*)
  cbus_input();
  //enable TI buffers, disable 74HC595 shift registers
  set_tigo(); 
*/
   //put TI on hold and enable 74HC595 shift registers
    set_tistop();
    //enable Arduino control bus signals (CE* and WE*)
    cbus_output();

  //enable TI interrupts (MBE*, WE* and A15 -> 74LS138 O0)
  //attachInterrupt(digitalPinToInterrupt(TI_INT), listen1771, RISING);
}

void loop() {

 /*

  if (cmd_FD1771 == 0xBB) {

    //delayMicroseconds(3);
    //delay(1); */

    /*for (int CByte = 0x0000; CByte <= 0x1FFF; CByte++) {
      fast_write(CByte,0xAA);
    }

    for (int CByte = 0x0000; CByte <= 0x1FFF; CByte++) {
      read_byte(CByte);
    }


/*
    cmd_FD1771 = 0;
    
    interrupts(); 
  } */
}

void listen1771() {
  noInterrupts();
  cmd_FD1771=0xBB;
}
