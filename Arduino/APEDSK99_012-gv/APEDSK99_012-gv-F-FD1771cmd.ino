void loop() {
 
  //check if TI_INT* has set by the GAL. This used to be an interrupt-driven approach but 
  //the Arduino interrupt response is painfully slow. Syncing with the TI became problematic. 
  //The polling alternative takes 400ns to respond, compared to ~2.2uS for the interrupt.

  noInterrupts();                                                                     //critical code section: "do not disturb" and ...
  while ( isHigh(MY_INT) ) {                                                          //... concentrate on GAL "write to DSR RAM" signal
  }

  MYstop();                                                                           //halt TI, enable 74HC595 shift registers and give Arduino RAM control
  interrupts();                                                                       //end of critical code section
  
  //read Command Register, stripping the unnecessary floppy bits but keeping command nybble
  currentFD1771cmd = read_DSRAM( WCOMND ) & B11110000;

  //the FD1771 "Force Interrupt" command is used to stop further command execution
  if ( currentFD1771cmd != FDINT ) {                                                  //do we need to exec some FD1771 command? 
    if ( newFD1771cmd = ( currentFD1771cmd != lastFD1771cmd ) ) {                     //new or continue previous FD1771 command?
      lastFD1771cmd = currentFD1771cmd;                                               //remember new command for next compare
      currentDSK = ( read_DSRAM( CRUWRI ) >> 1) & B00000011;                          //determine selected disk 
      currentDSK--;                                                                   //DSK1-3 -> DSK0-2 to reduce array sizes
 
      write_DSRAM( RSTAT, NOTREADY );                                                 //assume DSK "Not Ready"

      if ( activeDSK[currentDSK] ) {                                                  //is selected disk available? ...
        DSKx = SD.open( nameDSK[currentDSK], FILE_WRITE );                            //open SD DOAD file
        TNSECTS       = read_DSRAM( DSKprm +  (currentDSK * 6) ) * 256 +              //get total #sectors for this DSK
                        read_DSRAM( DSKprm + ((currentDSK * 6) + 1) );
        DOADidx       = read_DSRAM(WSECTR) * 256;                                     //calc absolute DOAD sector index
        DOADidx      += read_DSRAM(WSECTR + 1);

        if (DOADidx < long( TNSECTS ) ) {                                             //sector# within current DSK size?
          DOADidx *= NRBYSECT;                                                        //yes; calc absolute DOAD byte index for R/W      
          DSKx.seek( DOADidx );                                                       //set file position at byte index    
          write_DSRAM( RSTAT, NOERROR );                                              //reset "Not Ready" bit in Status Register 
        } else {
          currentFD1771cmd = FDINT;                                                   //sector# > 1440 -> exit
        }
      } else {
        currentFD1771cmd = FDINT;                                                     //...no; DSK not mapped -> exit   
      }
    }
    //***** FD1771 R/W commands
    
    switch ( currentFD1771cmd ) {

      case 0xD0:
      {
        noExec();
      }
      break;

      case 0x80:                                                                    //read sector
      {
        RWsector( READ );
      }
      break;

      case 0xA0:                                                                    //write sector
      {
        if ( protectDSK[currentDSK] != 0x50 ) {                                     //is DOAD write protected?
          RWsector( WRITE );                                                        //no; go ahead and write
        } else {
          write_DSRAM( RSTAT, PROTECTED );                                          //yes; set "Write Protect" bit in Status Register
          noExec();                                                                 //exit      
        }
      }
      break;

      default:                                                                      //catch-all safety
      {
        noExec();
      }
    } //end else R/W commands
  } else {                                                                            //no FD1771 stuff to execute, check out APEDSK99 commands
