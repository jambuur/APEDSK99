 //------------------------------------------------------------------------------------------- APEDSK99-specifc commands
      //check for APEDSK99-specfic commands
      ACcmd = Rbyte(ACOMND);
      if ( ACcmd != 0 ) {

        ANcmd = (ACcmd != ALcmd);                                             //new or continue previous APEDSK99 command?
        if (ANcmd) {                                                          //new command?
          ALcmd = ACcmd;                                                      //yes; remember new command for next compare
          cDSK = Rbyte(CALLBF);                                               //read target DSKx
        }
       
        //----------------------------------------------------------------- TI BASIC PDSK(), UDSK(), MDSK(), SDSK() and LDSK()
        switch ( ACcmd ) {

          case  1:                                          //UDSK(1):  Unprotect DSK1      
          case  2:                                          //UDSK(2):  Unprotect DSK2
          {
            if ( aDSK[cDSK] ) {                             //is the requested disk mapped to a DOAD?
              if ( ACcmd == 1 ) {                           //yes; PDSK?
                pDSK[cDSK] = 0x50;                          //Protect DSKx
              } else {
                pDSK[cDSK] = 0x20;                          //UDSK(); Unprotect DSKx
              }
              DSK[cDSK] = SD.open(nDSK[cDSK], O_WRITE);     //open DOAD file to change write protect status 
              DSK[cDSK].seek(0x10);                         //byte 0x10 in Volume Information Block stores Protected status 
              DSK[cDSK].write(pDSK[cDSK]);                  //update Protect status
            } else {
              Wbyte(CALLBF + 2, 0xFF);                      //no; return error flag
            }
            noExec();
          }
          break;      
 
          case 3:                                           //MDSK(): Map DOAD
          {
            char mDOAD[20] = "/DISKS/";                     //complete path + filename
            byte fPos;                                      //character position in filename from MDSK()
            for ( fPos = 7; fPos < 15; fPos++ ) {                                           
              byte fKar = Rbyte(CALLBF + (fPos - 5) );
              if ( fKar != ' ' ) {                          //if " ", the filename is < 8 characters ...
                  mDOAD[fPos] = fKar;
              } else {                                                                      
                break;                                      // ... and we need to get out
              }
            }                                                                              
            mDOAD[fPos + 1] = '\0';                         //terminate filename                                                                             
            strncat(mDOAD, ".DSK", 4);                      //add to path
            mDOAD[fPos + 4] = '\0';                         //terminate full path + filename
                        
            if ( SD.exists( mDOAD ) ) {                     //valid DOAD MS-DOS name?
              strncpy(nDSK[cDSK], mDOAD, 20);               //yes; assign to requested DSKx      
              aDSK[cDSK] = true;                            //flag active
              DSK[cDSK] = SD.open(nDSK[cDSK], O_READ);      //open new DOAD file to check write protect y/n
              DSK[cDSK].seek(0x10);                         //byte 0x10 in Volume Information Block stores Protect status
              pDSK[cDSK] = DSK[cDSK].read();                //0x50 || "P", 0x20 || " "
            } else {
              Wbyte(CALLBF + 2, 0xFF);                      //no; return error flag
            }
          }
          break;  
            
          case 4:                                            //SDSK(): Show DOAD mapping       
          {
            char sDOAD[20];                                
            if ( aDSK[cDSK] ) {                              //is the requested disk mapped to a DOAD?
             strcpy(sDOAD, nDSK[cDSK]);                      //yes; get current DOAD name     
            } else {
             strcpy(sDOAD, "/DISKS/<NO MAP>.DSK");           //no; indicate not mapped
            }
            
            for ( byte ii = 4; ii < 12; ii++ ){             //clear buffer space
              Wbyte(CALLBF + ii, ' ');
            }
            Wbyte(CALLBF + 2, cDSK+48+TIBias);               //DSKx # in ASCII + TI BASIC bias
            Wbyte(CALLBF + 3, '=' +   TIBias);               //"=" + TI BASIC bias  
            for ( byte ii = 4; ii < 12; ii++ ) {                                  
              Wbyte(CALLBF + ii, sDOAD[ii + 3] + TIBias);    //store mapping character in CALL buffer
              if ( sDOAD[ii + 3] == 46 ) {                   //if it's a "." we're done
                Wbyte(CALLBF + ii, ' ' + TIBias);            //but the "." needs to be a " "
                break;
              }
            } 
            noExec();
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

              for ( byte ii=2; ii < 18; ii++) {                                             //fill CALL() buffer with " "
                Wbyte(CALLBF + ii, 0x20 + TIBias);
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
                for ( byte ii = 14; ii < 18; ii++ ) {                                       //store ASCII file size in CALL buffer
                  Wbyte( CALLBF + ii, fSize[ii-14] + TIBias);
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

          case 6:
          {
            /*wdt_disable();
            wdt_enable(WDTO_15MS);
            while (1) {}*/
          }
          break;

          case 99:
          {
            dis_cbus();
            Serial.begin(9600);
            Serial.print("APEDSK99 (c)2020 by Jochen Buur");
            Serial.end();
            noExec();
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
