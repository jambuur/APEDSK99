 //------------------------------------------------------------------------------------------- APEDSK99-specifc commands
      //check for APEDSK99-specfic commands
      ACcmd = Rbyte(ACOMND);
      if ( ACcmd != 0 ) {

        ANcmd = (ACcmd != ALcmd);                                             //new or continue previous APEDSK99 command?
        if (ANcmd) {                                                          //new command?
          ALcmd = ACcmd;                                                      //yes; remember new command for next compare
        }
       
        //----------------------------------------------------------------- TI BASIC PDSK(), UDSK(), MDSK(), SDSK() and LDSK()
        switch ( ACcmd ) {

          for ( byte ii=2; ii < 18; ii++) {                                   //prep for M-/S-/LDSK: fill CALL() buffer with " " (TIBias)
            Wbyte(CALLBF + ii, 0x80);
          }

          case  1:                                                            //UDSK(1):  Unprotect DSK1
          case  2:                                                            //UDSK(2):  Unprotect DSK2
          case  3:                                                            //UDSK(3):  Unprotect DSK3      
          case  5:                                                            //PDSK(1):  Protect DSK1
          case  6:                                                            //PDSK(2):  Protect DSK2
          case  7:                                                            //PDSK(3):  Protect DSK3
          {
            cDSK = ACcmd & B00000011;                                         //strip U/P flag, keep DSKx
            if ( aDSK[cDSK] ) {
              if ( ACcmd & B00000100 ) {                                      //0x04 is Protect flag
                pDSK[cDSK] = 0x50;
              } else {
                pDSK[cDSK] = 0x20;
              }
              DSK[cDSK] = SD.open(nDSK[cDSK], FILE_WRITE);                    //open DOAD file to change write protect status 
              DSK[cDSK].seek(0x10);                                           //byte 0x10 in Volume Information Block stores Protected status 
              DSK[cDSK].write(pDSK[cDSK]);
            }
            noExec();
          }
          break;  

          case 9:                                                             //MDSK(): Map DOAD
           {
            char mDOAD[20] = "/DISKS/";
            byte mPos;
            for ( mPos = 2; mPos < 10; mPos++) {
              mDOAD[5 + mPos] = Rbyte(CALLBF + mPos);
              if ( mDOAD[5 + mPos] == ' ' ) {
                 break;
              }
            }
            mDOAD[5 + mPos] = '\0';
            strncat(mDOAD, ".DSK", 4);
            mDOAD[9 + mPos] = '\0';
  
            if ( SD.exists( mDOAD ) ) {                                       //exists?
              cDSK = Rbyte(CALLBF);                                           //yes; assign to requested DSKx
              strcpy(nDSK[cDSK], mDOAD);                        
              aDSK[cDSK] = true;                                              //flag active
              DSK[cDSK] = SD.open(nDSK[cDSK], O_READ);                        //open new DOAD file to check write protect y/n
              DSK[cDSK].seek(0x10);                                           //byte 0x10 in Volume Information Block stores Protect status
              pDSK[cDSK] = DSK[cDSK].read();                                  //0x50 || "P", 0x20 || " "
            } else {
              Wbyte(CALLBF, 0xFF);                                            //no; return error flag
            }  
            noExec();
          } 
          break;        
            
          case 10:                                                            //SDSK(): Show DOAD mapping       
          {
            char sDOAD[20];
            cDSK = Rbyte(CALLBF);                                             
            if ( aDSK[cDSK] ) {                                               //is the requested disk mapped to a DOAD?
             strcpy(sDOAD, nDSK[cDSK]);                                       //yes; get current DOAD name     
            } else {
             strcpy(sDOAD, "/DISKS/<NO MAP>.");                              //no; indicate not mapped
            }
            
            Wbyte(CALLBF    , cDSK+48+TIBias);                                //DSKx # in ASCII + TI BASIC bias
            Wbyte(CALLBF + 1, '=' +   TIBias);                                //"=" + TI BASIC bias  
            for ( byte ii = 2; ii < 10; ii++ ) {                                  
              Wbyte(CALLBF + ii, sDOAD[ii + 5] + TIBias);                     //store mapping character in CALL buffer
              if ( sDOAD[ii + 5] == 46 ) {                                    //if it's a "." we're done
                Wbyte(CALLBF + ii, ' ' + TIBias);                             //but the "." needs to be a " "
                break;
              }
            } 
            noExec();
          } 
          break;       

          case 11:                                                                          //LDSK(): List files on DOAD
          {            
            if ( Rbyte(CALLBF) != cDSK ) {                                                  //new DOAD?
                ANcmd = true;                                                               //yes; close possible open DOAD
                cDSK = Rbyte(CALLBF);                                                       //read new DOAD    
            }
     
            if ( aDSK[cDSK] ) { 
              if ( ANcmd ) {                                                                //yes; first run of LDSK()?
                DSK[cDSK] = SD.open(nDSK[cDSK], O_READ);                                    //yes; prep DOAD
                DSK[cDSK].seek(NRBYSECT);
              }
              
              unsigned long pFDR = (DSK[cDSK].read() << 8) + DSK[cDSK].read();              //16bits FDR pointer  
              if ( pFDR != 0 ) {                                                            //0x0000 means no more files              
                unsigned int cPos = DSK[cDSK].position();                                   //remember next FDR pointer
                DSK[cDSK].seek(pFDR * NRBYSECT);                                            //locate FDR within DOAD
                
                for ( byte ii=2; ii < 12; ii++ ) {                                  
                  Wbyte(CALLBF + ii, DSK[cDSK].read() + TIBias );                           //read/save filename characters in CALL buffer
                }
                Wbyte(CALLBF + 12, '-' + TIBias);                                           //separator space                     
                          
                DSK[cDSK].seek( DSK[cDSK].position() + 2);                                  //locate File Status Flags
                byte fStat = DSK[cDSK].read();               
                if ( fStat & B00000001 ) {
                  Wbyte(CALLBF + 13, 'P' + TIBias);                                         //"PROGRAM"
                } else if ( fStat & B00000010 ) {
                  Wbyte(CALLBF + 13, 'I' + TIBias);                                         //"INTERNAL"
                } else {
                  Wbyte(CALLBF + 13, 'D' + TIBias);                                         //"DISPLAY"
                }

                DSK[cDSK].seek( DSK[cDSK].position() + 1);                                  //locate total # of sectors
                char fSize[4];                                                              //ASCII store
                sprintf( fSize, "%3d", (DSK[cDSK].read() << 8) + (DSK[cDSK].read() + 1) );  //convert number to string
                
                for ( byte ii = 14; ii < 18130; ii++ ) {                                    //store ASCII file size in CALL buffer
                  Wbyte( CALLBF + ii, fSize[ii-14] + TIBias);
                }
                                  
                DSK[cDSK].seek(cPos);                                                       //locate next FDR
                
              } else {
                Wbyte(CALLBF + 2, 0xFF);                                                    //blank "floppy" or processed all FDR's
                noExec();
              }          
            } else {
              Wbyte(CALLBF + 2, 0xF0);                                                      //error; no mapped DOAD
              noExec();
            }
          }
          break;

        } //end switch accmd commands   
      } //end check APEDSK99-specific commands                                 
    } //end else 

    //----------------------------------------------------------------------------------------------- End of command processing, wait for next interrupt (TI write to DSR space)
    FD1771 = false;   //clear interrupt flag
    
    TIgo(); 

  } //end FD1771 flag check

} //end loop()
