void setup() {
  
  pinAsOutput(TI_BUFFERS);                                              //enable* or disable 74HCT541 address buffers and GAL outputs
  pinAsOutput(TI_READY);                                                //halt TI by making READY low; this enables the 74HC595 shift registers (and vice versa)

  TIstop();                                                             //halt TI, enable 74HC595 shift registers and give Arduino RAM control

  pinAsOutput(ETHSD_SS);                                                //initialise shield SS pin
  digitalHigh(ETHSD_SS);                                                //without this SPI subsystem won't work
  
  if ( !SD.begin(SPI_CS) ) {                                            //SD card present and can be initialized?              
    Flasher( 1 );                                                       //nope -> flash LED error 1
  }

  //74HC595 shift register control
  pinAsOutput(CLOCK);
  pinAsOutput(LATCH);
  pinAsOutput(DS);
 
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

  //initialise DSKx Active / Protect flags and save key parameters
  for ( byte ii = 0; ii < 3; ii++ ) {
    if ( SD.exists( nameDSK[ii] ) ) {                                   //does DOAD x exist?
      DSKx = SD.open( nameDSK[ii], FILE_READ );                         //yes; open DOAD file to check various parameters
      if ( !getDSKparms( ii ) ) {                                       //check DOAD size and if OK store DSK parameters
        activeDSK[ii] = true;                                           //flag active
        DSKx.seek( 0x10 );                                              //byte 0x10 in Volume Information Block stores Protected status
        protectDSK[ii] = DSKx.read();                                   //0x50 || "P" means disk is write protected        
      }
      DSKx.close();                                                     //close current SD DOAD file
    }
  } 
  
  //"initialize FD1771"
  FD1771reset();                                                        //"Restore" command
  noExec();                                                             //"no command" as default
    
  //GAL flag: A0-A2, A15(=0,MSB), MEMEN* and WE* -> IO7(pin 13)
  //signals a write to DSR space (aka CRU, FD1771 registers) ... 
  //... or read sector/track with R6 counter in RAM; see DSR source)
  pinAsInputPullUp(TI_INT);                                             //from Ethernet SS to detecting TI Interrupts 

  TIgo();                                                               //TI: take it away
  
} //end of setup()
