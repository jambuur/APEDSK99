//-DSR generic---------------------------------------------------------------------------------------- Hardware Error handling

void lDSR( char nDSR[] )
{
  char fDSR[12] = "";
  strncpy(fDSR, nDSR, 8);
  fDSR[8] = '\0';
  strncat(fDSR, ".DSR", 4);
  fDSR[12] = '\0';
  File iDSR = SD.open(fDSR, O_READ);
  if (iDSR) {
    for ( unsigned int ii = 0; ii < 0x2000; ii++ ) {
      Wbyte(ii, iDSR.read() );
    }
  iDSR.close();
  } else {
    eflash(2);                          //couldn't find DSR binary image: flash error 2
  }
  if ( Rbyte(0x0000) != 0xAA ) {        //check for valid DSR mark (>AA) at first DSR RAM byte
    eflash(3);                          //loading DSR unsuccessful -> flash error 3
  }
}

//flash error code:
//  flash            : SPI / SD Card fault/not ready
//  flash-flash      : can't read DSR binary image (/APEDSK99.DSR)
//  flash-flash-flash: no valid DSR header (>AA) at DSR RAM >0000

void eflash(byte error)                 //error routine: stuck in code flashing loop until reset
{
  //"no APEDSK99 for you", 
  //only vanilla TI console
  digitalHigh(TI_READY);                //enable TI
  
  while (true) { 
    for (byte flash = 0; flash < error; flash++)
    {
      digitalLow(CE);                   //set RAM CE* LOW, turns on LED
      delay(LED_ON);                    //LED is on for a bit
      
      digitalHigh(CE);                  //turn it off
      delay(LED_OFF);                   //LED is off for a bit
    }
    delay(LED_REPEAT);                  //allow human error interpretation
  }
}
