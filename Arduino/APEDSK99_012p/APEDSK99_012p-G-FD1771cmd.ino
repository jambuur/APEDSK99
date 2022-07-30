void loop() {
 
  //check if flag has set by interrupt routine
  if ( FD1771 ) {
    
    //read Command Register, stripping the unnecessary floppy bits but keeping command nybble
    currentFD1771cmd = read_DSRAM( WCOMND ) & B11110000;

    //the FD1771 "Force Interrupt" command is used to stop further command execution
    if ( currentFD1771cmd != FDINT ) {                                                  //do we need to exec some FD1771 command? 
      
      if ( newFD1771cmd = ( currentFD1771cmd != lastFD1771cmd ) ) {                     //new or continue previous FD1771 command?
        lastFD1771cmd = currentFD1771cmd;                                               //new; remember new command for next compare
      }

      //***** FD1771 Seek / Step
 
      if ( currentFD1771cmd < 0x80 ) {                                                  //step/seek commands? ...

        byte currentTrack = read_DSRAM( WTRACK );                                       //read current Track #
        byte newTrack     = read_DSRAM( WDATA );                                        //read new Track # (Seek)

        switch(currentFD1771cmd) {
         
          case 0x00:                                                                    //Restore            
          { 
            currentTrack = 0;                                                           //reset cTrack so Track Registers will be cleared after switch{}
            FD1771reset();
          }
          break;

          case 0x10:                                                                    //Seek
          {
            currentTrack   = newTrack;                                                  //prepare to update Track Register
            currentStepDir = ( newTrack > currentTrack );                               //set direction: HIGH = track #++, LOW = track #--
          } 
          break;
            
          case 0x20:                                                                    //Step
          case 0x30:                                                                    //Step+T
          {
            if ( currentStepDir == LOW ) {
              currentFD1771cmd = 0x70;                                                  //execute Step-Out+T
            } else {
              currentFD1771cmd = 0x50;                                                  //execute Step-In+T
            }
          }
          case 0x40:                                                                    //Step-In
          case 0x50:                                                                    //Step-In+T
          {
            if ( currentTrack < NRTRACKS ) {                                            //any tracks left to step to?
              currentTrack++;                                                           //yes; increase Track #
              currentStepDir = HIGH;                                                    //set stepping direction towards last track 
            }
          }
          break;

          case 0x60:                                                                    //Step-Out
          case 0x70:                                                                    //Step-Out+T
          {
            if ( currentTrack > 0 ) {                                                   //any tracks left to step to?
              currentTrack--;                                                           //yes; decrease Track #
              currentStepDir = LOW;                                                     //set stepping direction towards track 0
            }
          }
          break;
            
        } //end switch FCcmd
        write_DSRAM( WTRACK, currentTrack );                                            //update Track Register
        write_DSRAM( RTRACK, currentTrack );                                            //sync Track Registers
        noExec();                                                                       //prevent multiple step/seek execution                                        

      // end FCcmd < 0x80
      } else {                                                                          //... no; read/write commands
        
        //***** FD1771 R/W commands: prep
        
        if ( newFD1771cmd ) {                                                           //new command prep?
          currentDSK = ( read_DSRAM( CRUWRI ) >> 1) & B00000011;                        //yes; determine selected disk
          if ( activeDSK[currentDSK] ) {                                                //is selected disk available?
            write_DSRAM( RSTAT, NOERROR );                                              //yes; reset possible "Not Ready" bit in Status Register
            if ( currentFD1771cmd == 0xE0 || currentFD1771cmd == 0xF0 ) {               //R/W whole track?
              write_DSRAM( WSECTR, 0x00 );                                              //yes; start from sector 0
            }
            DSK[currentDSK] = SD.open( nameDSK[currentDSK], FILE_WRITE );               //open SD DOAD file
            DOADbyteidx = calcDOADidx();                                                //calc absolute DOAD byte index
            DSK[currentDSK].seek( DOADbyteidx );                                        //set to first absolute DOAD byte for R/W
          } else {
            if ( currentDSK != 0 ) {                                                    //ignore DSK0; either DSK1, DSK2 or DSK3 is not available
              write_DSRAM( RSTAT, NOTREADY );                                           //set "Not Ready" bit in Status Register
              currentFD1771cmd = FDINT;                                                 //exit 
            }
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
          case 0x90:                                                                    //read multiple sectors
          case 0xE0:                                                                    //read track    
          {
            RWsector( READ );
          }
          break;

          case 0xA0:                                                                    //write sector
          case 0xB0:                                                                    //write multiple sectors
          case 0xF0:                                                                    //write track
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

        } //end R/W switch
      } //end else R/W commands
    } else {                                                                            //no FD1771 stuff to execute, check out APEDSK99 commands
