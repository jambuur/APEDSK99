//----------------------------------------------------------------------------------------------- Setup

void setup() {

  //put TI on hold and enable 74HC595 shift registers
  TIstop();

  //SD card present and can be initialized?
  if ( !SD.begin(SPI_CS) ) {                          //nope -> flash LED error 1
    eflash(1);
  }
  
  //74HC595 shift register control
  pinAsOutput(DS);
  pinAsOutput(LATCH);
  pinAsOutput(CLOCK);

  //74LS138 interrupt: TI WE*, MBE* and A15 -> O0 output
  //a write to DSR space (aka CRU, FD1771 registers; or sector/track Read through R6 counter in RAM, see DSR source)
  pinAsInput(TI_INT);

  //--------------------------------------------------------------------------------------------- DSR initialisation
  
  //read DSR binary from SD and write into DSR RAM
  lDSR("APEDSK99");

  //initialise DSKx Active and Protect flags
  for ( byte ii = 1; ii < 4; ii++ ) {
    if ( SD.exists(nDSK[ii])) {                       //does DOAD x exist?
      aDSK[ii] = true;                                //yes; flag as such
      DSK[ii] = SD.open(nDSK[ii], O_READ);            //open DOAD file to check write protect y/n
      DSK[ii].seek(0x10);                             //byte 0x10 in Volume Information Block stores Protected status
      pDSK[ii] = DSK[ii].read();                      //0x50 || "P" means disk is write protected 
      DSK[ii].close();                                //close current SD DOAD file
    }
  }
  
  //--------------------------------------------------------------------------------------------- Let's go
  //"initialize FD1771":
  FDrstr();                                           //"Restore" command
  noExec();                                           //"no command" as default

  //enable TI interrupts (MBE*, WE* and A15 -> 74LS138 O0)
  //direct interrupt register access for speed
  EICRA |= B00000010;                                 //sense falling edge on INT0 pin

  TIgo();                                             //TI: take it away

} //end of setup()
