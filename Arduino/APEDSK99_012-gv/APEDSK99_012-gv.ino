/* APEDISK99 - Myarc Geneve 9640 version
  This sketch emulates 3 TI99/4a DS/SD/80tracks disk drives, including the ability to load and save Disk-On-A-Disk (DOAD) files 
  via the FTP protocol. It also provides BASIC access to NTP date/time information. Bonus CALL: real lower case characters
  
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

  using the SDfat library (much faster, less bugs and substantially reduced memory footprint):
  https://github.com/greiman/SdFat

  $Author: Jochen Buur $
  $Date: February 2023 $
  $Revision: 0.12s $
  
  This software is freeware and can be modified, re-used or thrown away without any restrictions.
  Use this code at your own risk.
  I'm not responsible for any bad effect or damages caused by this software
*/

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
byte MAC[] =    { 0x06, 0xAA, 0xBB, 0xCC, 0xDD, 0x99 };                       //EthernetShield MAC address
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
#include <EEPROM.h>                                                           //EEPROM access: save DSR filename to load at powerup / reset and network config parameters

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
