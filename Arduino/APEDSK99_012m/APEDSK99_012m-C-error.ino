//-DSR generic---------------------------------------------------------------------------------------- Hardware Error handling

//flash error code:
//  flash                   : SPI / SD Card fault/not ready
//  flash-flash             : can't read DSR binary image (/APEDSK99.DSR)
//  flash-flash-flash       : no valid DSR header (>AA) at DSR RAM >0000
void eflash(byte error)
{
  //"no APEDSK99 for you" but let user still enjoy a vanilla TI console
  digitalHigh(TI_READY);      //enable TI but ...
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
