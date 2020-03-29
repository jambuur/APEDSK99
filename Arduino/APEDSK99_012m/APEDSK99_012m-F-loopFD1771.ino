//------------------------------------------------------------------------------------------------ Loop
void loop() {
 
  //check if flag has set by interrupt routine
  if (FD1771) {
    
    //read Command Register, stripping the unnecessary floppy bits but keeping command nybble
    FCcmd = Rbyte(WCOMND) & B11110000;

    //the FD1771 "Force Interrupt" command is used to stop further command execution
    if ( FCcmd != FDINT ) { //do we need to do anything?
      
      FNcmd = (FCcmd != FLcmd);                             //new or continue previous FD1771 command?
      if (FNcmd) {                                          //new command?
        FLcmd = FCcmd;                                      //yes; remember new command for next compare
      }
      //----------------------------------------------------------------------------------------- FD1771 Seek / Step
      if ( FCcmd < 0x80 ) {                                 //step/seek commands?

        byte cTrack = Rbyte(WTRACK);                             //read current Track #
        byte nTrack = Rbyte(WDATA);                              //read new Track # (Seek)

        switch(FCcmd) {
         
          case 0x00:                                        //Restore            
          { 
            cTrack = 0;                                     //reset cTrack so Track Registers will be cleared after switch{}
            FDrstr();
          }
          break;

          case 0x10:                                        //Seek
          {
            cTrack = nTrack;                                //prepare to update Track Register
            cDir = (nTrack > cTrack);                       //set direction: HIGH=track # increase, LOW=track # decrease
          } 
          break;
            
          case 0x20:                                        //Step
          case 0x30:                                        //Step+T
          {
            if ( cDir == LOW ) {
              FCcmd = 0x70;                                 //execute Step-Out+T
            } else {
              FCcmd = 0x50;                                 //execute Step-In+T
            }
          }
          case 0x40:                                        //Step-In
          case 0x50:                                        //Step-In+T
          {
            if ( cTrack < NRTRACKS ) {                      //any tracks left to step to?
              cTrack++;                                     //yes; increase Track #
              cDir = HIGH;                                  //set stepping direction towards last track 
            }
          }
          break;

          case 0x60:                                        //Step-Out
          case 0x70:                                        //Step-Out+T
          {
            if ( cTrack > 0 ) {                             //any tracks left to step to?
              cTrack--;                                     //decrease Track #
              cDir = LOW;                                   //set stepping direction towards track 0
            }
          }
          break;
            
        } //end switch FCcmd
        Wbyte(WTRACK, cTrack);                              //update Track Register
        Wbyte(RTRACK, cTrack);                              //sync Track Registers
        noExec();                                           //prevent multiple step/seek execution                                        

      // end FCcmd < 0x80
      } else {  // read/write commands
        
        //----------------------------------------------------------------------------------------- FD1771 R/W commands: prep
        if ( FNcmd ) {                                          //new command prep
          cDSK = ( Rbyte(CRUWRI) >> 1) & B00000011;             //yes do some prep; determine selected disk
          if ( aDSK[cDSK] ) {                                   //is selected disk available?
            Wbyte(RSTAT, NOERROR);                              //reset possible "Not Ready" bit in Status Register
            if ( FCcmd == 0xE0 || FCcmd == 0xF0 ) {             // R/W whole track?
              Wbyte(WSECTR, 0x00);                              //yes; start from sector 0
            }
            DSK[cDSK] = SD.open(nDSK[cDSK], O_READ | O_WRITE);  //open SD DOAD file
            Dbtidx = cDbtidx();                                 //calc absolute DOAD byte index
            DSK[cDSK].seek(Dbtidx);                             //set to first absolute DOAD byte for R/W
          } else {
            if ( cDSK != 0 ) {                                  //ignore DSK0; either DSK1, DSK2 or DSK3 is not available
              Wbyte(RSTAT, NOTREADY);                           //set "Not Ready" bit in Status Register
              FCcmd = FDINT;                                    //exit 
            }
          }
        }
        
        //----------------------------------------------------------------------------------------- FD1771 R/W commands
        switch (FCcmd) {  //switch R/W commands

          case 0xD0:
          {
            noExec();
          }
          break;

          case 0x80:                                      //read sector
          case 0x90:                                      //read multiple sectors
          case 0xE0:                                      //read track    
          {
            RWsector( true );
          }
          break;

          case 0xA0:                                      //write sector
          case 0xB0:                                      //write multiple sectors
          case 0xF0:                                      //write track
          {
            if ( !pDSK[cDSK] ) {                          //is DOAD write protected?
              RWsector( false );                          //no; go ahead and write
            } else {
              Wbyte(RSTAT, PROTECTED);                    //yes; set "Write Protect" bit in Status Register
              FCcmd = FDINT;                              //exit      
            }
          }
          break;

        } //end R/W switch
      } //end else R/W commands

    //end we needed to do something
    } else {  //check out APEDSK99 commands  
