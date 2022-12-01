    //***** APEDSK99-specfic commands
    
    currentA99cmd = read_DSRAM( ACOMND );
    if ( currentA99cmd != 0 ) {
      
      if ( newA99cmd = (currentA99cmd != lastA99cmd) ) {                                                  //new or continue previous APEDSK99 command?
        lastA99cmd = currentA99cmd;                                                                       //new; remember new command for next compare
        currentDSK = read_DSRAM( CALLBF );                                                                //read target DSKx
        currentDSK--;                                                                                     //DSK1-3 -> DSK0-2 to reduce array sizes
        CALLstatus( AllGood );                                                                            //start with clean execution slate
      }
      
      switch ( currentA99cmd ) {
        
        case  2:                                                                                          //PDSK()    
        case  4:                                                                                          //UDSK()
        {
          if ( activeDSK[currentDSK] ) {                                                                  //is the requested disk mapped to a DOAD? ...
            if ( currentA99cmd == 2 ) {                                                                   //yes; PDSK?
              protectDSK[currentDSK] = 0x50;                                                              //Protect DSKx
            } else {
              protectDSK[currentDSK] = 0x20;                                                              //Unprotect DSKx
            }
            DSK[currentDSK] = SD.open( nameDSK[currentDSK], FILE_WRITE);                                  //open DOAD file to change write protect status 
            DSK[currentDSK].seek(0x10);                                                                   //byte 0x10 in Volume Information Block stores Protected status 
            DSK[currentDSK].write( protectDSK[currentDSK] );                                              //update Protect status
          } else {
            CALLstatus( DOADNotMapped );                                                                  //... no; set error flag
          }
          noExec();
        }
        break;      
    
        case  6:                                                                                          //MDSK()
        case 10:                                                                                          //RDSK()
        case 22:                                                                                          //FGET()
        case 24:                                                                                          //FPUT()                                                                        
        {
          char DOADfullpath[20] = "/DISKS/\0";                                                            //complete path + filename
          char DOADfilename[13];                                                                          //just the filename for the FTP server
          byte ii;
          for ( ii = 0; ii < 8; ii++ ) {                                                                  //max 8 characters for MSDOS filename
            byte cc = read_DSRAM( CALLBF + (ii + 2) );                                                    //read character from APEDSK99 CALL buffer
            if ( cc != 0x80 ) {                                                                           //if (" " + TIBias), the filename is < 8 characters ...
                DOADfilename[ii] = cc;                                                                    //add next character to FTP filename
            } else {                                                                      
              break;                                                                                      // ... and we need to get out
            }
          }                                                                              
          clrCALLbuffer();                                                                                //clear CALL buffer
          DOADfilename[ii] = '\0';                                                                        //terminate FTP filename
          strncat( DOADfilename, ".DSK", 4 );                                                             //add file extenstion to filename
          strncat( DOADfullpath, DOADfilename, ++ii + 4 );                                                //add filename to path                                                                                                               
           
          boolean existDOAD = SD.exists( DOADfullpath );                                                  //existing DOAD flag
          byte protectDOAD;                                                                               //Protected flag
          if ( existDOAD ) {                                                                              //does DOAD exist?>
            DSK[currentDSK] = SD.open( DOADfullpath, FILE_READ);                                          //yes; open DOAD file
            DSK[currentDSK].seek(0x10);                                                                   //byte 0x10 in Volume Information Block stores status
            protectDOAD = DSK[currentDSK].read();                                                         //store Protected flag
          }

          if ( currentA99cmd == 6 ) {                                                                     //MDSK() ?
            if ( existDOAD ) {                                                                            //does DOAD exist? ...
              strncpy( nameDSK[currentDSK], DOADfullpath, ii + 11 );                                      //yes; assign to requested DSKx   
              activeDSK[currentDSK] = true;                                                               //flag as active
              protectDSK[currentDSK] = protectDOAD;                                                       //set Protected status: 0x50 || "P", 0x20 || " "
            } else {
              CALLstatus( DOADNotFound );                                                                 //... no; return error flag
            }

          } else if ( currentA99cmd == 10 ) {                                                             //RDSK() ?                                                                                     
            if ( existDOAD ) {                                                                            //does DOAD exist? ...
              if ( protectDOAD == 0x20 ) {                                                                //yes; is DOAD unProtected? ...
                for ( byte ii= 0; ii < 3; ii++ ) {
                  if ( strcmp(DOADfullpath, nameDSK[ii]) == 0 ) {                                         //is DOAD currently mapped?
                    activeDSK[ii] = false;                                                                //yes; flag DSKx non-active
                  }
                  DSK[currentDSK].close();
                  SD.remove( DOADfullpath );                                                              //yes; delete DOAD from SD card
                }
              } else {
                CALLstatus( Protected );                                                                  //... no; Protected, report error to CALL()
              }
            } else {
              CALLstatus( DOADNotFound );                                                                 //... no; return error flag
            }

          } else {                                                                                        //FGET() and FPUT()         
            EtherStart();                                                                                 //start Ethernet client

            if ( !ftp.connect(ftpserver, user, pass) ) {                                                  //if we can't connect to FTP server ...
              CALLstatus( FNTPConnect );                                                                  //... flag error ...     
              currentA99cmd = 0;                                                                          //... and cancel further command execution
            }
            
            if ( currentA99cmd == 24 ) {                                                                  //FPUT()?
              if ( existDOAD ) {                                                                          //yep; does DOAD exist ...
                DSK[currentDSK].seek(0);                                                                  //yes; go to start of file (we were at 0x10)
                ftp.store( DOADfilename, DSK[currentDSK] );                                               //yep; send DOAD to ftp server
              } else {
                CALLstatus( DOADNotFound );                                                               //no; report error to CALL()
              }
            } else if ( currentA99cmd == 22 ) {                                                           //FGET()?
              if ( existDOAD && protectDOAD == 0x50) {                                                    //yep; is existing DOAD Protected?
                CALLstatus( Protected );                                                                  //yes; report error to CALL()
              } else {
                DSK[currentDSK] = SD.open( "/_FT", FILE_WRITE );                                          //no; open temporary FTP file
                ftp.retrieve( DOADfilename, DSK[currentDSK] );                                            //get DOAD from FTP server
                if ( DSK[currentDSK].size() != 0 ) {                                                      //size > 0 = transfer successful? ...
                  if ( existDOAD ) {                                                                      //yes; does DOAD already exist?
                    SD.remove( DOADfullpath );                                                            //yes -> delete
                  }
                  DSK[currentDSK].rename( DOADfullpath );                                                 //rename temp FTP file to proper DOAD
                } else {
                  DSK[currentDSK].remove();                                                               //... no; remove tmp FTP file
                  CALLstatus( DOADNotFound );                                                             //report transfer error (check ownership/group/rights on FTP server)
                }
              }
            }
          }                                                             
          EtherStop();                                                                                    //stop Ethernet client
          noExec();                                                                                       //we're done
        }
        break;

        case 8:                                                                                           //LDSK()
        {            
          clrCALLbuffer();                                                                                //clear CALL buffer (longer / shorter filenames)
               
          if ( activeDSK[currentDSK] ) { 
            if ( newA99cmd ) {                                                                            //yes; first run of LDSK()?
              DSK[currentDSK] = SD.open( nameDSK[currentDSK], FILE_READ );                                //yes; prep DOAD
              DSK[currentDSK].seek( NRBYSECT );                                                           //2nd sector contains the File Descriptor Records (FDR)
            }
    
            unsigned long FDR = ( DSK[currentDSK].read() << 8 ) + DSK[currentDSK].read();                 //16bits FDR pointer  
            if ( FDR != 0 && read_DSRAM(CALLST) ==  AllGood ) {                                           // !0x0000( = no more files) AND !ENTER from DSR?             
              unsigned int currentPosition = DSK[currentDSK].position();                                  //remember next FDR pointer
              DSK[currentDSK].seek( FDR * NRBYSECT );                                                     //locate FDR within DOAD
              for ( byte ii=2; ii < 12; ii++ ) {                                  
               write_DSRAM( CALLBF + ii, DSK[currentDSK].read() + TIBias );                               //read/save filename characters in CALL buffer
              }                 
                        
              DSK[currentDSK].seek( DSK[currentDSK].position() + 2 );                                     //locate File Status Flags
              byte filetype = DSK[currentDSK].read();               
              if ( filetype & B00000001 ) {
                write_DSRAM( CALLBF + 13, 'P' + TIBias );                                                 //"PROGRAM"
              } else if ( filetype & B00000010 ) {
                write_DSRAM( CALLBF + 13, 'I' + TIBias );                                                 //"INTERNAL"
              } else {
                write_DSRAM( CALLBF + 13, 'D' + TIBias );                                                 //"DISPLAY"
              }

              char filesize[4];                                                                           //ASCII store
              DSK[currentDSK].seek( DSK[currentDSK].position() + 1 );                                     //locate total # of sectors
              sprintf( filesize, "%3u", (DSK[currentDSK].read() << 8) + (DSK[currentDSK].read() + 1) );   //convert number to string
              for ( byte ii = 14; ii < 17; ii++ ) {                                                       //store ASCII file size in CALL buffer
                write_DSRAM( CALLBF + ii, filesize[ii-14] + TIBias );
              }
                                
              DSK[currentDSK].seek( currentPosition );                                                    //locate next FDR
            } else {                                       
              CALLstatus( More );                                                                         //blank "floppy" or processed all FDR's
              noExec();                                                                   
            }          
          } else {
            CALLstatus( DOADNotMapped );                                                                  //error; no mapped DOAD
            noExec();
          }
        }
        break;

        case 12:                                                                                          //SDSK()  
        {
          if ( newA99cmd ) {
            currentDSK = 0;                                                                               //first run make sure we start with DSK1
            gii = 0;                                                                                      //keep track when to display SDISK info (1 row = 2x 16 char fields
          }
          
          char DOADfullpath[20];
          clrCALLbuffer();                                                                                //clear CALL buffer (second part of row is blank)

          if ( gii < 6 ) {                                                                                //3 rows, 1 row is 2x16char fields                
            if ( !(gii & B00000001) ) {                                                                   //if field == even we need to display mapping, otherwise time/date
              if ( activeDSK[currentDSK] ) {                                                              //is the requested disk mapped to a DOAD?

                DSK[currentDSK] = SD.open( nameDSK[currentDSK], FILE_READ );
                                                            
                unsigned int countOnes = 0;               
                  
                DSK[currentDSK].seek( 0x12 );                                                             //# of formatted sides
                byte maxbitmap = DSK[currentDSK].read() * 45;                                             //# of Sector Bitmap bytes to process for SD or DS
                DSK[currentDSK].seek( 0x38 );                                                             //1st Sector Bitmap byte for side 0
                 
                for ( byte ii = 0; ii < maxbitmap -1 ; ii++ ) {                                           //sum all set bits (=sectors used) for all Sector Bitmap bytes
                  byte bitmap = DSK[currentDSK].read();
                  for ( byte jj = 0; jj < 8; jj++ ) {                                       
                    countOnes += ( bitmap >> jj ) & 0x01;                                                 //"on the One you hear what I'm sayin'"
                  }
                }

                char freesectors[4];                                                                      //ASCII store
                sprintf( freesectors, "%3u", (maxbitmap * 8) - countOnes );                               //convert number of free sectors to string
                for ( byte ii = 15; ii < 18; ii++ ) {                                                     //store ASCII file size in CALL buffer
                  write_DSRAM( CALLBF + ii, freesectors[ii-15] + TIBias );
                }
                strncpy( DOADfullpath, nameDSK[currentDSK], 20 );                                         //get current DOAD name     

                if ( protectDSK[currentDSK] == 0x50 ) {
                write_DSRAM( CALLBF + 13, 'P' + TIBias );                                                 //indicate DOAD is Protected or ...
                } else {
                  write_DSRAM( CALLBF + 13, 'U' + TIBias );                                               //... Unprotected
                }
              } else {
                strncpy( DOADfullpath, "/DISKS/<NO MAP>.DSK", 20 );                                       //... no; indicate not mapped
              }
          
              write_DSRAM( CALLBF + 2, '1' + currentDSK + TIBias );                                       //DSK# + TI BASIC bias
              write_DSRAM( CALLBF + 3, '=' + TIBias );                                            
              for ( byte ii = 4; ii < 12; ii++ ) {                                  
                write_DSRAM( CALLBF + ii, DOADfullpath[ii + 3] + TIBias );                                //store mapping character in CALL buffer
                if ( DOADfullpath[ii + 3] == 46 ) {                                                       //if it's a "." we're done
                  write_DSRAM( CALLBF + ii, ' ' + TIBias );                                               //replace "." with " "
                  break;
                }
              } 
            } else {                                                                                      //uneven field, display FAT MODIFIED time and date
              if ( activeDSK[currentDSK] ) {                                                              //is the requested disk mapped to a DOAD?
                readFATts();                                                                              //yes; read FAT time/date
                converTD();                                                                               //convert to string
                for ( byte ii = 3; ii < 15; ii++ ) {
                  write_DSRAM( CALLBF+ii, TimeDateASC[ii-3] + TIBias );                                   //write string to CALL buffer for display without year
                }
                write_DSRAM( CALLBF+15, TimeDateASC[14] + TIBias);                                        //only space to display last 2 year digits
                write_DSRAM( CALLBF+16, TimeDateASC[15] + TIBias); 
              } 
              if ( currentDSK < 2 ) {                                                                     //must stay within 0-2 for noExec() to properly close current open DSK[]
                currentDSK++;                                                                             //next DSK  
              }
            }
            gii++;                                                                                        //next screen field
          } else { 
            CALLstatus( More );                                                                           //processed DSK[1-3]
            noExec();                                                                         
          }      
        }                                                            
        break;       

        case 14:                                                                                          //SDIR(): Show DOAD's in /DISKS/ on SD
        {
          clrCALLbuffer();                                                                                //clear CALL buffer
          if ( newA99cmd ) {                                                                              //first run of SDIR()?
            SDdir = SD.open( "/DISKS/" );                                                                 //yes; open directory
          }        

          File NextFile = SDdir.openNextFile();                                                           //get next file
          if ( NextFile && read_DSRAM(CALLST) ==  AllGood ) {                                             //valid file AND !ENTER from DSR?
            char DOADfilename[13] = "\0";                                                                 //"clear" array for shorter filenames not displaying leftover parts
            NextFile.getName( DOADfilename, 13 );                                                         //copy filename ... 
            for ( byte ii = 2; ii < 14; ii++ ) {                                                          //... and write to buffer
              write_DSRAM( CALLBF + ii, DOADfilename[ii-2] + TIBias );
            } 
            NextFile.seek( 0x12 );                                                                        //byte >12 in VIB stores # of sides (>01 or >02)
            write_DSRAM( CALLBF + 15, NextFile.read() + (48 + TIBias) );                                  //change to ASCII and add TI screen bias
            write_DSRAM( CALLBF + 16, 'S' + TIBias );                                                     //"SIDED"
            NextFile.close();                                                                             //prep for next file
          } else {                                                                                        //... no
            SDdir.close();
            CALLstatus( More );                                                                           //done all files
            noExec(); 
          }
        }
        break;    


        case 16:                                                                                          //AHLP()
        {
          if ( newA99cmd ) {                                                                              //first run make sure we start with 1st message
            gii = 0;
          }

          if ( gii < 36 ) {                                                                               //34x 16char arrays
            for ( byte ii = 2; ii < 18; ii++ ) {
              write_DSRAM( CALLBF + ii, pgm_read_byte( &CALLhelp[gii][ii-2] ) + TIBias );                 //read character array from PROGMEM and write to CALL buffer
            }
            gii++;                                                                                        //next
          } else {
            CALLstatus( More );                                                                           //done all help messages
            noExec();
          }
        }
        break;

        case 18:                                                                                          //TIME()
        case 19:                                                                                          //DSR: update FAT DSK date/time
        {
          clrCALLbuffer();                                                                                //clear CALL buffer
          EtherStart();                                                                                   //start Ethernet client
          
          if ( getNTPdt() == FNTPConnect ) {                                                              //check if NTP is available
            CALLstatus( FNTPConnect );                                                                    //no; report error
          } else if ( currentA99cmd == 18 ) {                                                             //TIME()
            converTD();                                                                                   //get full time/date string         
            for ( byte ii = 2; ii < 18; ii++ ) {
              write_DSRAM( CALLBF + ii, TimeDateASC[ii-2] + TIBias );                                     //copy string to CALL buffer                                   
            }
            CALLstatus( NTPStamp );                                                                       //done with a one-liner 
          } else if ( currentA99cmd == 19 ) {                                                             //DSR DSK NTP update (format, write / save file)                                                                           
            currentDSK = ( read_DSRAM( CRUWRI ) >> 1 ) & B00000011;                                       //determine selected disk in DSR command   
            currentDSK--;                                                                                 //DSK1-3 -> DSK0-2 to reduce array sizes
            if ( protectDSK[currentDSK] != 0x50 ) {                                                       //DSK protected?
              DSK[currentDSK] = SD.open( nameDSK[currentDSK], FILE_WRITE );
              writeFATts();                                                                               //no; update DOAD FAT date/time
            }  
          }
          EtherStop();                                                                                    //stop Ethernet client
          //---
          CALLstatus( AllGood );                                                                          //----IF THIS LINE IS COMMENTED OUT, TIME WILL PRINT DATE/TIME BY DEFAULT
          //---
          noExec(); 
        }
        break;
        
        case 26:                                                                                          //ADSR()
        {                                                                                 
          char DSRtoload[13];                                                                             //filename new DSR
          char DSRcurrent[13] = "\0";                                                                     //filename current DSR in EEPROM
          for ( byte ii = 2; ii < 10; ii++ ) {                                                            //read file name from CALL buffer
            DSRtoload[ii - 2] = read_DSRAM( CALLBF + ii );                                                //get new DSR from CALL buffer
          }                                                          
          DSRtoload[8] = '\0';                                                                        
          strncat( DSRtoload, ".DSR", 4 );                                                                //complete filename

          if ( !SD.exists(DSRtoload) ) {                                                                  //does new DSR exist?
            CALLstatus( DSRNotFound );                                                                    //nope; error
            noExec();
          } else {
            EEPROM.get( EEPROMDSR, DSRcurrent );                                                          //get current DSR from EEPROM
            if ( strcmp(DSRcurrent, DSRtoload) != 0) {                                                    //compare to new DSR; same?
              EEPROM.put( EEPROMDSR, DSRtoload );                                                         //no; write new DSR to EEPROM
            }
            currentA99cmd = 20;                                                                           //valid DSR file; fall through to reset to activate
          }
        }

        case 20:                                                                                          //ARST(): reset current DSR    
        { 
          if ( currentA99cmd == 20 ) {                                                                    //seems double-up but is needed for ADSR fall-through
            TIgo();                                                                                       //release TI
            APEDSK99reset();                                                                              //reset APEDSK99
          }
        }
        break;

        default:                                                                                          //catch-all safety
        {
          noExec();
        }
        
      }//end ACcmd switch
    }//end APEDSK99 commands
  }//end do we need to exec some FD1771 command? 
  
  //***** End of command processing, wait for next GAL interrupt (TI write to DSR space)

//  interrupts();
  TIgo();                                                                                                 //TI, the floor is yours
 
} //end loop() 
