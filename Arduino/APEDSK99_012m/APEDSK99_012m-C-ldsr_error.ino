//-DSR generic---------------------------------------------------------------------------------------- Hardware Error handling

void lDSR( char nDSR[9] )
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
    //couldn't find DSR binary image: flash error 2
    eflash(2);
  }
  //check for valid DSR mark (>AA) at first DSR RAM byte
  if ( Rbyte(0x0000) != 0xAA ) {
    //loading DSR unsuccessful -> flash error 3
    eflash(3);
  }
}

//flash error code:
//  flash                   : SPI / SD Card fault/not ready
//  flash-flash             : can't read DSR binary image (/APEDSK99.DSR)
//  flash-flash-flash       : no valid DSR header (>AA) at DSR RAM >0000
void eflash(byte error)
{
  //"no APEDSK99 for you" but let user still enjoy a vanilla TI console
  digitalHigh(TI_READY);      //enable TI
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
