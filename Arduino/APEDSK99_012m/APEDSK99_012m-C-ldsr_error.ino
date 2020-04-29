//-DSR generic---------------------------------------------------------------------------------------- Hardware Error handling

//load DSR of choice 
boolean lDSR( char nDSR[] )
{
  char fDSR[18] = "/";
  strncpy(fDSR, nDSR, 18);                              //copy startup / reset / CALL ADSR() file name     
  strncat(fDSR, ".DSR", 4);                             //add extension
  File iDSR = SD.open(fDSR, O_READ);                    //open DSR file and if exists copy to RAM
  if (iDSR) {
    for ( unsigned int ii = 0; ii < 0x2000; ii++ ) {
      Wbyte(ii, iDSR.read() );
    }
  iDSR.close();
  return (0);                                           //success
  } else {
    return(1);                                          //indicate failure
  }
}

//reset Arduino properly via watchdog timer
void APEDSK99rst (void) {
  wdt_disable();                                        //disable Watchdog (just to be sure)                              
  wdt_enable(WDTO_15MS);                                //enable it and set delay to 15ms
  while(1) {}                                           //self-destruct in 15ms
}

void rtcEnd( void ) {
  TWCR = bit(TWEN) | bit(TWSTO);                      //no interrupts, Set Stop(release bus)
  TWCR = bit(TWSTO);                                  //no interrupts, Set Stop(release bus), disable TWI interface
}

//flash error code:
//  flash            : SPI / SD Card fault/not ready
//  flash-flash      : can't read DSR binary image (/APEDSK99.DSR)
//  flash-flash-flash: no valid DSR header (0xAA) at DSR RAM 0x0000)

void eflash(byte error)                                 //error routine: stuck in code flashing loop until reset
{
  //"no APEDSK99 for you", 
  //only vanilla TI console
  digitalHigh(TI_READY);                                //enable TI
  
  while (true) { 
    for (byte flash = 0; flash < error; flash++)
    {
      digitalLow(CE);                                   //set RAM CE* LOW, turns on LED
      delay(LED_ON);                                    //LED is on for a bit
      
      digitalHigh(CE);                                  //turn it off
      delay(LED_OFF);                                   //LED is off for a bit
    }
    delay(LED_REPEAT);                                  //allow human error interpretation
  }
}
