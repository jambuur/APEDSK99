void setup() {
  
  TIstop();                                                             //put TI on hold and enable 74HC595 shift registers

  pinAsOutput(ETHSD_SS);                                                //initialise shield SS pin
  digitalHigh(ETHSD_SS);                                                //without this SPI subsystem won't work
  
  if ( !SD.begin(SPI_CS) ) {                                            //SD card present and can be initialized?              
    Flasher( 1 );                                                       //nope -> flash LED error 1
  }

  //74HC595 shift register control
  pinAsOutput(DS);
  pinAsOutput(LATCH);
  pinAsOutput(CLOCK);
 
  //read DSR binary from SD and write into DSR RAM
  if ( EEPROM.read(EEPROMDSR) == 0xFF ) {                               //invalid DSR (i.e. virgin Uno EEPROM or invalid DSR filename)?
    EEPROM.put( EEPROMDSR, "APEDSK99.DSR\0" );                          //yes, put default in EEPROM ...
  }
  char DSRtoload[13] = "\0";                                            //fresh char array
  EEPROM.get( EEPROMDSR, DSRtoload );                                   //get DSR filename from EEPROM
  if ( SD.exists(DSRtoload) ) {                                         //does it exist? ...
    File fileDSR = SD.open( DSRtoload, FILE_READ );                     //yes; write DSR data to APEDSK99 RAM
    for ( unsigned int ii = 0x4000; ii < 0x6000; ii++ ) {
      write_DSRAM( ii, fileDSR.read() );                                                
    }
    
    fileDSR.close();
  } else {
    EEPROM.write( EEPROMDSR, 0xFF);                                     //... no; mark DSR invalid so default gets restored at next boot
    Flasher( 2 );                                                       //flash error 2
  }
  
  if ( read_DSRAM(0x4000) != 0xAA ) {                                   //valid DSR mark (>AA) at first DSR RAM byte?  
    EEPROM.write( EEPROMDSR, 0xFF);                                     //no; mark DSR invalid
    Flasher( 3 );                                                       //loading DSR unsuccessful -> flash error 3
  }

  //initialise DSKx Active and Protect flags
  for ( byte ii = 1; ii < 4; ii++ ) {
    if ( SD.exists( nameDSK[ii] ) ) {                                   //does DOAD x exist?
      activeDSK[ii] = true;                                             //yes; flag active
      DSK[ii] = SD.open( nameDSK[ii], FILE_READ );                      //open DOAD file to check write protect y/n
      DSK[ii].seek( 0x10 );                                             //byte 0x10 in Volume Information Block stores Protected status
      protectDSK[ii] = DSK[ii].read();                                  //0x50 || "P" means disk is write protected 
      DSK[ii].close();                                                  //close current SD DOAD file
    }
  }
  
  //"initialize FD1771":
  FD1771reset();                                                        //"Restore" command
  noExec();                                                             //"no command" as default
  
  //enable TI interrupts (MBE*, WE* and A15 -> 74LS138 O0)
  //direct interrupt register access for speed
  EICRA |= B00000010;                                                   //sense falling edge on INT0 pin

  //74LS138 interrupt: TI WE*, MBE* and A15 -> O0 output
  //a write to DSR space (aka CRU, FD1771 registers; 
  //or sector/track Read through R6 counter in RAM, see DSR source)
  pinAsInput(TI_INT);                                                   //from Ethernet SS to detecting TI Interrupts 

  TIgo();                                                               //TI: take it away

} //end of setup()
