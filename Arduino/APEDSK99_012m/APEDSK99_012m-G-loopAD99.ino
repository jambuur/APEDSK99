 //------------------------------------------------------------------------------------------- APEDSK99-specifc commands
      //check for APEDSK99-specfic commands
      ACcmd = Rbyte(ACOMND);
      if ( ACcmd != 0 ) {

        ANcmd = (ACcmd != ALcmd);                                             //new or continue previous APEDSK99 command?
        if (ANcmd) {                                                          //new command?
          ALcmd = ACcmd;                                                      //yes; remember new command for next compare
        }
       
        //----------------------------------------------------------------- TI BASIC PDSK(), UDSK(), CDSK(), SDSK() and FDSK()
        switch ( ACcmd ) {

          case  1:                                                            //UDSK(1):  Unprotect DSK1
          case  2:                                                            //UDSK(2):  Unprotect DSK2
          case  3:                                                            //UDSK(3):  Unprotect DSK3      
          case  5:                                                            //PDSK(1):  Protect DSK1
          case  6:                                                            //PDSK(2):  Protect DSK2
          case  7:                                                            //PDSK(3):  Protect DSK3
          {
            cDSK = ACcmd & B00000011;                                         //strip U/P flag, keep DSKx
            if ( aDSK[cDSK] ) {
              DSK[cDSK] = SD.open(nDSK[cDSK], O_READ | O_WRITE);              //open DOAD file to change write protect status 
              DSK[cDSK].seek(0x28);                                           //byte 0x28 in Volume Information Block stores APEDSK99 adhesive tab status 
              if ( ACcmd & B00000100 ) {                                      //Protect bit set?
                DSK[cDSK].write('P');                                         //yes; apply adhesive tab
                pDSK[cDSK] = true;  
              } else {
                DSK[cDSK].write(' ');                                         //no; remove adhesive tab
                pDSK[cDSK] = false;        
              }
            } 
            noExec();
          }
          break;  

          case 9:                                                             //MDSK(): Map DOAD
           {
            String mDOAD = "";
            for ( byte ii = 2; ii < 10; ii++ ) {                              //merge CALL CDSK characters into string
              mDOAD += char( Rbyte(DTCDSK + ii) );
            }          
            mDOAD.trim();                                                     //remove leading / trailing spaces
            mDOAD = "/DISKS/" + mDOAD + ".DSK";                               //construct full DOAD path
            if ( SD.exists( mDOAD ) ) {                                       //exists?
              cDSK = Rbyte(DTCDSK);                                           //yes; assign to requested DSKx
              nDSK[cDSK] = mDOAD;
              aDSK[cDSK] = true;
              DSK[cDSK] = SD.open(nDSK[cDSK], O_READ);                        //open new DOAD file to check write protect y/n
              DSK[cDSK].seek(0x28);                                           //byte 0x28 in Volume Information Block stores APEDSK99 adhesive tab status
              pDSK[cDSK] = ( DSK[cDSK].read() == 0x50 );                      //0x50 || "P" means disk is write APEDSK99 protected
              DSK[cDSK].close();                                              //close new DOAD file
            } else {
              Wbyte(DTCDSK, 0xFF);                                            //no; return error flag
            }    
            noExec();
          } 
          break;        
            
          case 10:                                                            //SDSK(): Show DOAD mapping       
          {
            String sDOAD = "";
            char cDot = "";
            cDSK = Rbyte(DTCDSK);                                             //is the requested disk mapped to a DOAD?
            if ( aDSK[cDSK] ) {
             sDOAD = nDSK[cDSK];                                               //yes; get current DOAD name
            } else {
              sDOAD = "/DISKS/<NO MAP>";                                       //no; indicate as such
            }
            Wbyte(DTCDSK    , cDSK+48+TIBias);                                //drive # in ASCII + TI BASIC bias
            Wbyte(DTCDSK + 1, '=' +   TIBias);                                //"=" in ASCII + TI BASIC bias
            for ( byte ii = 2; ii < 10; ii++ ) {
              cDot = sDOAD.charAt(ii+5) + TIBias;                              //read character and add TI BASIC bias      
              if ( cDot != char(142) ) {                                      //is it "."?
                Wbyte(DTCDSK + ii, cDot);                                     //no; prepare next character
              } else {
                ii = 11;                                                      //yes; don't print, end loop          
              }    
            }
            noExec();
          } 
          break;       

          case 11:
          {            
            if ( Rbyte(DTCDSK) != cDSK ) {                                                  //new DOAD?
                ANcmd = true;                                                               //yes; close possible open DOAD
                cDSK = Rbyte(DTCDSK);                                                       //read new DOAD    
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

                for ( byte ii=2; ii < 18; ii++) {                                           //fill buffer with " "
                  Wbyte(DTCDSK + ii, 0x80);
                }
                
                for ( byte ii=2; ii < 12; ii++ ) {                                  
                  Wbyte(DTCDSK + ii, DSK[cDSK].read() + TIBias );                           //read/save filename characters in CALL buffer
                }
                Wbyte(DTCDSK + 12, '-' + TIBias);                                           //separator space                     
                          
                DSK[cDSK].seek( DSK[cDSK].position() + 2);                                  //locate File Status Flags
                byte fStat = DSK[cDSK].read();               
                if ( fStat & B00000001 ) {
                  Wbyte(DTCDSK + 13, 'P' + TIBias);                                         //"PROGRAM"
                } else if ( fStat & B00000010 ) {
                  Wbyte(DTCDSK + 13, 'I' + TIBias);                                         //"INTERNAL"
                } else {
                  Wbyte(DTCDSK + 13, 'D' + TIBias);                                         //"DISPLAY"
                }

                DSK[cDSK].seek( DSK[cDSK].position() + 1);                                  //locate total # of sectors
                String fSize = String( ((DSK[cDSK].read() << 8) + DSK[cDSK].read() + 1) );  //convert 16bits int to string 
                byte fLength = (char)fSize.length();                                        //get ASCII string length
                Wbyte(DTCDSK + 16, fSize.charAt(fLength-1) + TIBias);                       //least significant digit in ASCII to CALL buffer 
                if ( fLength > 1 ) {
                  Wbyte (DTCDSK + 15, fSize.charAt(fLength - 2) + TIBias );                 //"10's" digit in ASCII to CALL buffer
                }
                if ( fLength == 3) {
                  Wbyte (DTCDSK + 14, fSize.charAt(fLength - 3) + TIBias);                  //"100's" digit in ASCII to CALL buffer
                }
                    
                DSK[cDSK].seek(cPos);                                                       //locate next FDR
                
              } else {
                Wbyte(DTCDSK + 2, 0xFF);                                                    //blank "floppy" or processed all FDR's
                noExec();
              }          
            } else {
              Wbyte(DTCDSK + 2, 0xF0);                                                      //error; no mapped DOAD
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
