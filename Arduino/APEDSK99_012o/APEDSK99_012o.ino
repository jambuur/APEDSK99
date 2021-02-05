/* APEDISK99
  This sketch emulates 3 TI99/4a DS/SD disk drives, including the ability to load and save Disk-On-A-Disk (DOAD) files via the FTP protocol
  
  Shopping list:
  - Arduino Uno
	- Ethernet shield (https://store.arduino.cc/usa/arduino-ethernet-rev3-without-poe) with  SD card functionality
	- APEDSK99 shield (github.com/jambuur/APEDSK99)

  The Arduino RAM interface is based on Mario's EEPROM programmer:
  http://github.com/mkeller0815/MEEPROMMER

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

  using the Industrial Shields FTP library (straightforward, small and fast):
  https://github.com/IndustrialShields/arduino-FTP

  using the Industrial Shields NTP library (straightforward and small):
  https://github.com/IndustrialShields/arduino-NTP

  using the SDfat library (much faster, less bugs and substantially reduced memory footprint:
  https://github.com/greiman/SdFat

  $Author: Jochen Buur $
  $Date: April 2019-December 2020 $
  $Revision: 0.12o $
  
  This software is freeware and can be modified, re-used or thrown away without any restrictions.
  Use this code at your own risk.
  I'm not responsible for any bad effect or damages caused by this software
*/

//*****

//SPI functions
#include <SPI.h>

//SPI subsystem enable
#define ETHSD_SS 10

//SPI card SS pin
#define SPI_CS 10                                                             //jumper cable EthernetShield D4 -> APEDSK99 D10

//Ethernet card CS pin (shared with TI_INT)
#define ETH_CS 2                                                              //jumper cable EthernetShield 10 -> EthernetShield D2       

//SD functions
#include "SdFat.h"
SdFat SD;
SdFile file;

#include <Ethernet.h>
byte MAC[] =    { 0x06, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };                       //EthernetShield MAC address
EthernetClient  client;
EthernetClient  ftpControl;
EthernetClient  ftpData;

//watchdog timer handling for reset/load DSR
#include <avr/wdt.h>

//Industrial Shields FTP library
#include <FTP.h>
FTP ftp(ftpControl, ftpData);

//Industrial Shields NTP library
#include <NTP.h>
EthernetUDP udp;
NTP ntp(udp);

//store DSR filename in EEPROM to survive boot
#include <EEPROM.h>                                                           //EEPROM access: save DSR filename to load at powerup / reset

//*****

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

//74HC595 shift-out definitions
#define CLOCK     17                                                          //PC3
#define LATCH     18                                                          //PC4
#define DS        19                                                          //PC5

//IO lines for Arduino RAM control
#define CE  14                                                                //PC0  LED flashes for both Arduino and TI RAM access
#define WE  16	                                                              //PC2

//IO lines for TI99/4a control
#define TI_READY       3                                                      //PD0; TI READY line + enable/disable 74HC595 shift registers
#define TI_INT      	 2                                                      //PD2; GAL16V8 interrupt (DSR write & A15); shared with Ethernet CS (ETH_CS) 
#define TI_BUFFERS    15                                                      //PC1; 74LS541 buffers enable/disable

//error blinking parameters
#define LED_ON       500                                                      //on
#define LED_OFF      250                                                      //off
#define LED_REPEAT  1500                                                      //delay between sequence
