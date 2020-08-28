 //------------------------------------------------------------------------------------------- APEDSK99-specifc commands
      //check for APEDSK99-specfic commands
      ACcmd = Rbyte(ACOMND);
      if ( ACcmd != 0 ) {
        
        ANcmd = (ACcmd != ALcmd);                                                           //new or continue previous APEDSK99 command?
        if (ANcmd) {                                                                        //new command?
          ALcmd = ACcmd;                                                                    //yes; remember new command for next compare
          cDSK = Rbyte(CALLBF);                                                             //read target DSKx
          if ( ACcmd != 3 && ACcmd != 6 && ACcmd !=8 ) {                                    //clear CALL buffer but not for MDSK(), ADSR() and SRTC()
            for ( byte ii=2; ii < 18; ii++) {                                               //fill CALL() buffer with " "
              Wbyte(CALLBF + ii, 0x20 + TIBias);
            }
          }
        }
        //----------------------------------------------------------------- TI BASIC PDSK(), UDSK(), MDSK(), SDSK() and LDSK()
        switch ( ACcmd ) {
      
          case  1:                                                                          //PDSK(x):  Protect DSKx      
          case  2:                                                                          //UDSK(x):  Unprotect DSKx
          {
            if ( aDSK[cDSK] ) {                                                             //is the requested disk mapped to a DOAD?
              if ( ACcmd == 1 ) {                                                           //yes; PDSK?
                pDSK[cDSK] = 0x50;                                                          //Protect DSKx
              } else {
                pDSK[cDSK] = 0x20;                                                          //Unprotect DSKx
              }
              DSK[cDSK] = SD.open(nDSK[cDSK], O_WRITE);                                     //open DOAD file to change write protect status 
              DSK[cDSK].seek(0x10);                                                         //byte 0x10 in Volume Information Block stores Protected status 
              DSK[cDSK].write(pDSK[cDSK]);                                                  //update Protect status
            } else {
              Wbyte(CALLBF + 2, 0xFF);                                                      //no; return error flag
            }
            noExec();
          }
          break;      
      
          case 3:                                                                           //MDSK(): Map DOAD
          {
            char mDOAD[20] = "/DISKS/";                                                     //complete path + filename
            byte fPos;                                                                      //character position in filename from MDSK()
            for ( fPos = 7; fPos < 15; fPos++ ) {                                           
              byte fKar = Rbyte(CALLBF + (fPos - 5) );
              if ( fKar != ' ' ) {                                                          //if " ", the filename is < 8 characters ...
                  mDOAD[fPos] = fKar;
              } else {                                                                      
                break;                                                                      // ... and we need to get out
              }
            }                                                                              
            mDOAD[fPos + 1] = '\0';                                                         //terminate filename                                                                             
            strncat(mDOAD, ".DSK", 4);                                                      //add to path
                        
            if ( SD.exists( mDOAD ) ) {                                                     //valid DOAD MS-DOS name?
              strncpy(nDSK[cDSK], mDOAD, 20);                                               //yes; assign to requested DSKx      
              aDSK[cDSK] = true;                                                            //flag as active
              DSK[cDSK] = SD.open(nDSK[cDSK], O_READ);                                      //open new DOAD file to check write protect y/n
              DSK[cDSK].seek(0x10);                                                         //byte 0x10 in Volume Information Block stores Protect status
              pDSK[cDSK] = DSK[cDSK].read();                                                //0x50 || "P", 0x20 || " "
            } else {
              Wbyte(CALLBF + 2, 0xFF);                                                      //no; return error flag
            }
            noExec();
          }
          break;  
            
          case 4:                                                                           //SDSK(): Show DOAD mapping       
          {
            char sDOAD[20];                                
            unsigned int nOnes = 0;
            if ( aDSK[cDSK] ) {                                                             //is the requested disk mapped to a DOAD?
        
              DSK[cDSK] = SD.open(nDSK[cDSK], O_READ);                                      //yes ...
              DSK[cDSK].seek(0x12);                                                         //# of formatted sides
              byte tSect = DSK[cDSK].read() * 45;                                           //# of Sector Bitmap bytes to process for SD or DS
              DSK[cDSK].seek(0x38);                                                         //1st Sector Bitmap byte for side 0

              for ( byte ii = 0; ii < tSect -1 ; ii++) {                                    //sum all set bits (=sectors used) for all Sector Bitmap bytes
                byte bSBM = DSK[cDSK].read();
                for ( byte jj = 0; jj < 8; jj++ ) {                                         
                  nOnes += ( bSBM >> jj) & 0x01;                                            //"on the one you hear what I'm sayin'"
                }
              }
              char dSize[4];                                                                //ASCII store
              sprintf( dSize, "%3d", (tSect * 8) - nOnes );                                 //convert: -> number of free sectors -> string
              for ( byte ii = 15; ii < 18; ii++ ) {                                         //store ASCII file size in CALL buffer
                Wbyte(CALLBF + ii, dSize[ii-15] + TIBias);
              }
              strncpy(sDOAD, nDSK[cDSK], 20);                                               //get current DOAD name     

              if ( pDSK[cDSK] == 0x50 ) {
              Wbyte(CALLBF + 13, 'P' + TIBias);                                             //indicate DOAD is Protected
              } else {
                Wbyte(CALLBF + 13, 'U' + TIBias);                                           //indicate DOAD is Unprotected
              }
            } else {
              strncpy(sDOAD, "/DISKS/<NO MAP>.DSK", 20);                                    //... no; indicate not mapped
            }
            
            Wbyte(CALLBF + 2, '=' + TIBias);                                                //"=" + TI BASIC bias  
            for ( byte ii = 4; ii < 12; ii++ ) {                                  
              Wbyte(CALLBF + ii, sDOAD[ii + 3] + TIBias);                                   //store mapping character in CALL buffer
              if ( sDOAD[ii + 3] == 46 ) {                                                  //if it's a "." we're done
                Wbyte(CALLBF + ii, ' ' + TIBias);                                           //replace "." with " "
                break;
              }
            } 
            noExec();                                                                       //we're done
          } 
          break;       
      
          case 5:                                                                           //LDSK(): List files on DOAD
          {            
            if ( Rbyte(CALLBF) != cDSK ) {                                                  //different DSKx while ">" for previous DSKx?
              cDSK = Rbyte(CALLBF);                                                         //yes; read new DSKx
              ANcmd = true;                                                                 //close possible open DOAD 
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
                for ( byte ii = 14; ii < 17; ii++ ) {                                       //store ASCII file size in CALL buffer
                  Wbyte(CALLBF + ii, fSize[ii-14] + TIBias);
                }
                                  
                DSK[cDSK].seek(cPos);                                                       //locate next FDR
                
              } else {
                Wbyte(CALLBF + 2, 0xF0);                                                    //blank "floppy" or processed all FDR's
                noExec();
              }          
            } else {
              Wbyte(CALLBF + 2, 0xFF);                                                      //error; no mapped DOAD
              noExec();
            }
          }
          break;

          case 6:                                                                           //ADSR():  load DSR
          {                                                                                 
            char mDSR[13] = "DSR/";                                                         //complete path + filename
            for ( byte ii = 2; ii < 10; ii++ ) {                                            //read file name from CALL buffer
             mDSR[ii + 2] = Rbyte(CALLBF + ii);
            }                                                                  
                                                                                   
            mDSR[12] = '\0';                                                                //terminate filename                                                                             
            if ( lDSR(mDSR) == 1 ) {                                                        //load DSR and if unsuccessful ...
              Wbyte(CALLBF + 2, 0xFF);                                                      //... return error code
              noExec();
              break;
            }     
          }                                                                                 //fall through to reset    
          
          case 7:                                                                           //ARST(): reset APEDSK99
          {
            TIgo();                                                                         //release TI
            APEDSK99rst();                                                                  //reset APEDSK99
          }
          break;

          case 8:                                                                           //SRTC(): set RTC date/time
          {
            char tRTC[13] = "";                                                             //data To RTC from CALLBF
            tRTC[12] = '\0';                                                                //terminate string
            for ( byte ii = 2; ii < 14; ii++ ) {                                            //check if supplied chars are between "0" - "9"
              tRTC[ii - 2] = Rbyte(CALLBF + ii);
              if ( tRTC[ii - 2] < 48 || tRTC[ii - 2] > 57 ) {                               //no
                Wbyte(CALLBF + 2, 0xFF);                                                    //store error code
                break;                                                                      //break out loop
              } 
            }
            if ( Rbyte(CALLBF + 2) != 0xFF ) {                                              //still good to go?
              byte cASC = tRTC[2];                                                          //yes; save 1st char of MM ...
              tRTC[2] = '\0';                                                               //... and write string delimiter for DD ...
              byte Day = atoi(&tRTC[0]);                                                    //... convert DD to number ...
              tRTC[2] = cASC;                                                               //... and restore 1st MM char
              cASC = tRTC[4];                                                               //same for MM, YYYY, HH and MM
              tRTC[4] = '\0';
              byte Month = atoi(&tRTC[2]);
              tRTC[4] = cASC;
              cASC = tRTC[8];
              tRTC[8] = '\0';
              unsigned int Year = atoi(&tRTC[4]);
              tRTC[8] = cASC; 
              cASC = tRTC[10];
              tRTC[10] = '\0';
              byte Hour = atoi(&tRTC[8]);
              tRTC[10] = cASC;
              cASC = tRTC[12];
              tRTC[12] = '\0';
              byte Minute = atoi(&tRTC[10]);
              tRTC[12] = cASC;

              dis_cbus();                                                                   //RTC I2C bus uses DS and LATCH so disable RAM control
              rtc.begin();                                                                  //start I2C RTC comms
              boolean eRTC = false;                                                         //RTC health flag
              if ( rtc.isrunning() ) {
                rtc.adjust( DateTime(Year, Month, Day, Hour, Minute, 0) );                  //adjust RTC
              } else {
                eRTC = true;                                                                //flag RTC error (can't write to RAM with bus disabled)
              }
              rtcEnd();                                                                     //stop I2C comms
              ena_cbus();                                                                   //enable RAM control bus
              if ( eRTC ) {
                Wbyte(CALLBF + 2, 0xFF);                                                    //flag RTC error
              }
            } else {
              Wbyte(CALLBF + 2, 0xFF);                                                      //flag CALL error
            }
            noExec();                                                                       //either way we're done  
          }
          break;

          case 9:                                                                           //GRTC(): Get RTC date/time
          {
            char fRTC[17] = "";                                                             //data From RTC to CALLBF
            char tChar[3] = "";
            char fChar[5] = "";
            dis_cbus();                                                                     //RTC I2C bus uses DS and LATCH so disable RAM control
            rtc.begin();                                                                    //start I2C RTC comms
            boolean eRTC = false;                                                           //RTC health flag
            if ( rtc.isrunning() ) {
              DateTime now = rtc.now();                                                     //get current date/time     
              sprintf(tChar, "%02d", now.day() );                                           //convert day to ASCII ...
              strncpy(fRTC, tChar, 2);                                                      //... copy to date/time string ..
              fRTC[2] = '/';
              fRTC[3] = '\0';                                                               //... put delimiter for month ...
              tChar[0] = '\0';                                                              //... reset convert array for next round
              sprintf(tChar, "%02d", now.month() );                                         //same for MM, YYYY, HH and MM
              strncat(fRTC, tChar, 2);
              fRTC[5] = '/';
              fRTC[6] = '\0';
              tChar[0] = '\0';
              sprintf(fChar, "%4d", now.year() ); 
              strncat(fRTC, fChar, 4);
              fRTC[10] = ' ';
              fRTC[11] = '\0';
              sprintf(tChar, "%02d", now.hour() ); 
              strncat(fRTC, tChar, 2);
              fRTC[13] = ':';
              fRTC[14] = '\0';
              tChar[0] = '\0';
              sprintf(tChar, "%02d", now.minute() );
              strncat(fRTC, tChar, 2);  
            } else {
              eRTC = true;                                                                  //flag RTC error (can't write to RAM with bus disabled)
            }
            rtcEnd();                                                                       //stop I2C comms
            ena_cbus();                                                                     //enable RAM control bus           
            if ( !eRTC ) {                                                                  //all good?
              for ( byte ii = 2; ii < 19; ii++ ) {                                          //yes; write date/time string to CALLBF
                Wbyte(CALLBF + ii, fRTC[ii - 2] + TIBias);
              } 
            } else {
              Wbyte(CALLBF + 2, 0xFF);                                                      //no; flag RTC error
            }
            noExec();
          }
          break;

          case 10:                                                                          //SDIR(): Show DOAD's in /DISKS/ on SD
          {
            if ( ANcmd ) {                                                                  //yes; first run of SDIR()?
              SDdir.close();                                                                //in case another command was issued when previous SDIR() was still active (multiple screens)
              SDdir = SD.open("/DISKS/");                                                   //yes; open directory
            }

            File nDOAD = SDdir.openNextFile();                                              //get next file
            if ( nDOAD) {                                                                   //valid?
              char fName[13];                                                               //yes 
              strncpy(fName, nDOAD.name(), 12);                                             //copy filename ...
              for ( byte ii = 2; ii < 14; ii++) {                                           //... and write to buffer
                Wbyte(CALLBF + ii, fName[ii-2] + TIBias);
              } 
              Wbyte(CALLBF + 16, 'S' + TIBias);                                             //"SIDED"
              if ( nDOAD.size() < 180000 ) {                                                //single (90KB)?
                Wbyte(CALLBF + 15, '1' + TIBias);                                           //yep -> "1S"
              } else {      
                Wbyte(CALLBF + 15, '2' + TIBias);                                           //no -> "2S"
              }
              nDOAD.close();                                                                //prep for next file
            } else {                                                                        //no
              SDdir.close();
              Wbyte(CALLBF + 2, 0xF0);                                                      //signal done all files
              noExec(); 
            }
          }
          break;    

          default:                                                                          //catch-all safety
          {
            noExec();
          }
          
        }//end ACcmd switch
      }//end APEDSK99 commands
    }//end we needed to do something
    
    //----------------------------------------------------------------------------------------------- End of command processing, wait for next interrupt (TI write to DSR space)
    FD1771 = false;   //clear interrupt flag
    
    TIgo(); 
    
  } //end FD1771 flag check
} //end loop()
