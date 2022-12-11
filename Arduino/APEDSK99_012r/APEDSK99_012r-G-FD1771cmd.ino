void loop() {
 
  //check if TI_INT* has set by GAL. This used to be an interrupt-driven approach until I realised that Arduino interrupt response is painfully slow. 
  //Syncing with the TI became problematic. The polling alternative takes 400ns to respond, compared to ~2.2uS for the interrupt.

  noInterrupts();                                                                     //critical code section: "do not disturb" and ...
  while ( isHigh(TI_INT) ) {                                                          //... concentrate on GAL "write to DSR RAM" signal
  }

  TIstop();                                                                           //halt TI, enable 74HC595 shift registers and give Arduino RAM control
  interrupts();                                                                       //end of critical code section
  
  //read Command Register, stripping the unnecessary floppy bits but keeping command nybble
  currentFD1771cmd = read_DSRAM( WCOMND ) & B11110000;

  //the FD1771 "Force Interrupt" command is used to stop further command execution
  if ( currentFD1771cmd != FDINT ) {                                                  //do we need to exec some FD1771 command? 
    if ( newFD1771cmd = ( currentFD1771cmd != lastFD1771cmd ) ) {                     //new or continue previous FD1771 command?
      lastFD1771cmd = currentFD1771cmd;                                               //new; remember new command for next compare
      currentDSK = ( read_DSRAM( CRUWRI ) >> 1) & B00000011;                          //determine selected disk 
      currentDSK--;                                                                   //DSK1-3 -> DSK0-2 to reduce array sizes

      if ( activeDSK[currentDSK] ) {                                                  //is selected disk available ...
        write_DSRAM( RSTAT, NOERROR );                                                //yes; reset possible "Not Ready" bit in Status Register
        if ( currentFD1771cmd == 0xE0 || currentFD1771cmd == 0xF0 ) {                 //R/W whole track?
          write_DSRAM( WSECTR, 0x00 );                                                //yes; start from sector 0
        }

        DSKx = SD.open( nameDSK[currentDSK], FILE_WRITE );                            //open SD DOAD file
        TNSECTS  = read_DSRAM( DSKprm + (currentDSK * 6) ) * 256 +                    //total #sectors for this DSK
                   read_DSRAM( DSKprm + ((currentDSK * 6) + 1) );
        NRSECTS  = read_DSRAM( DSKprm + ((currentDSK * 6) + 2) );                     //#sectors/track      
        DOADbyteidx  = read_DSRAM(WSECTR) * 256;                                      //calc absolute DOAD sector index
        DOADbyteidx += read_DSRAM(WSECTR + 1);

        if ( DOADbyteidx < long(TNSECTS) ) {                                          //#sector within current DSK size?
          DOADbyteidx *= NRBYSECT;                                                    //yes;set to first absolute DOAD byte for R/W      
          DSKx.seek( DOADbyteidx );                                                   //seek byte index       
        } else {
          write_DSRAM( RSTAT, NOTREADY );                                             //no; set "Not Ready" bit in Status Register
          currentFD1771cmd = FDINT;                                                   //exit
        }
      } else {
        write_DSRAM( RSTAT, NOTREADY );                                               //... no; set "Not Ready" bit in Status Register
        currentFD1771cmd = FDINT;                                                     //exit 
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
//        case 0x90:                                                                    //read multiple sectors
//        case 0xE0:                                                                    //read track    
      {
        RWsector( READ );
      }
      break;

      case 0xA0:                                                                    //write sector
//        case 0xB0:                                                                    //write multiple sectors
//        case 0xF0:                                                                    //write track
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
