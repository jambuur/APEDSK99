    //***** APEDSK99-specfic commands
    
    currentA99cmd = read_DSRAM( ACOMND );
    if ( currentA99cmd != 0 ) {
      
      if ( newA99cmd = (currentA99cmd != lastA99cmd) ) {                                                  //new or continue previous APEDSK99 command?
        lastA99cmd = currentA99cmd;                                                                       //new; remember new command for next compare
        currentDSK = read_DSRAM( CALLBF );                                                                //read target DSKx
        if ( currentDSK > 0 ) {
          currentDSK--;                                                                                   //DSK1-3 -> DSK0-2 to reduce array sizes
        }
        CALLstatus( AllGood );                                                                            //start with clean execution slate
      }

      switch ( currentA99cmd ) {
        
        case  1:                                                                                          //PDSK: set DSK protection    
        case  2:                                                                                          //UDSK: reset DSK protection
        {
          if ( activeDSK[currentDSK] ) {                                                                  //is the requested disk mapped to a DOAD? ...
            if ( currentA99cmd == 1 ) {                                                                   //yes; PDSK?
              protectDSK[currentDSK] = 0x50;                                                              //Protect DSKx
            } else {
              protectDSK[currentDSK] = 0x20;                                                              //Unprotect DSKx
            }
            DSKx = SD.open( nameDSK[currentDSK], FILE_WRITE);                                             //open DOAD file to change write protect status 
            DSKx.seek(0x10);                                                                              //byte 0x10 in Volume Information Block stores Protected status 
            DSKx.write( protectDSK[currentDSK] );                                                         //update Protect status
          } else {
            CALLstatus( DOADNotMapped );                                                                  //... no; set error flag
          }
          noExec();
        }
        break;      
        
        case 3:                                                                                           //LDSK(): show files on selected DSK
        {            
          if ( activeDSK[currentDSK] ) {                                                                  //mapped DSK?
           
            if ( newA99cmd ) {                                                                            //first run of LDSK()?
              DSKx = SD.open( nameDSK[currentDSK], FILE_READ );                                           //yes; prep DOAD
              DSKx.seek( NRBYSECT );                                                                      //2nd sector contains the File Descriptor Records (FDR)  
            }
                      
            unsigned int FDR = ( DSKx.read() << 8 ) + DSKx.read();                                        //read 16bits FDR pointer
            if ( FDR != 0 && read_DSRAM(CALLST) ==  AllGood ) {                                           // !0x0000( = no more files) AND !ENTER from DSR?
      
              clrCALLbuffer();                                                                            //clear CALL buffer for next entry to display
  
              unsigned int currentPosition = DSKx.position();                                             //remember next FDR pointer
              DSKx.seek( long(FDR * NRBYSECT) );                                                          //locate FDR within DOAD
              for ( byte ii=0; ii < 10; ii++ ) {                                  
                write_DSRAM( CALLBF + ii, DSKx.read() + TIBias );                                         //read/save filename characters in CALL buffer
              }               
  
              char filetype[16] = "\0";                                                                   //ASCII store for display                                                      
              char tfile[6] = "\0";                                                                       //temporary ASCII store
              
              DSKx.seek( DSKx.position() + 2 );                                                           //byte >0C: file type 
              byte ftype = DSKx.read();
              boolean protect = (ftype & 0x08) != 0;                                                      //bit 4 set means file is protected
              ftype &= 0x83;                                                                              //strip protection indication
              if ( ftype == 0 ) {                                                                         //0x00 is DISPLAY
                strncpy( filetype, "D/F\0", 4);
              } else if ( ftype == 2 ) {                                                                  //0x02 is INTERNAL
                strncpy( filetype, "I/F\0", 4);             
              } else if ( ftype == 128 ) {                                                                //0x80 is VARIABLE
                strncpy( filetype, "D/V\0", 4);
              } else if ( ftype == 130 ) {
                strncpy( filetype, "I/V\0", 4);
              } else if ( ftype == 1 ) {                                                                  //0x01 is PROGRAM
                strncpy( filetype, "PRG\0", 4);
              } else {
                strncpy( filetype, " ? \0", 4);                                                           //expect the unexpected
              }
  
              DSKx.seek( DSKx.position() + 4 );                                                           //byte >11: record length
              sprintf( tfile, "%3u", DSKx.read() );                                                       //format record length to 3char ASCII
              if ( ftype != 1 ) {                                                                         //not applicable for PROGRAM files -> spaces
                strncat( filetype, tfile, 3);
              } else {
                strncat( filetype, "   ", 3);
              }
              strncat( filetype, " \0", 2);
  
              DSKx.seek( DSKx.position() - 4 );                                                           //bytes >0E, >0F: #sectors
              unsigned int tsectors = (DSKx.read() * 256) + (DSKx.read() + 1);
              sprintf( tfile, "%3u", tsectors);                                                           //format #sectors to 3char ASCII
              strncat( filetype, tfile, 3);
              strncat( filetype, " \0", 2);          
              sprintf( tfile, "%5lu", long(tsectors * 256) );                                             //format #bytes to 5char ASCII (do we need 6?)
              strncat( filetype, tfile, 6);
              filetype[16] = '\0';
  
              for ( byte ii=11; ii < 27; ii++ ) {                                  
                write_DSRAM( CALLBF + ii, filetype[ii-11] + TIBias );                                     //save file characteristics in CALL buffer for display
              }    
  
             if ( protect == true ) {                                                                     //indicate if file is protected 
                write_DSRAM( CALLBF + 28, 'P' + TIBias);
              }
                                     
              DSKx.seek( currentPosition );                                                               //locate next FDR
            
            } else {                                       
              CALLstatus( More );                                                                         //blank "floppy" or processed all FDR's
              noExec();                                                                   
            }          
          } else {
            CALLstatus( DOADNotMapped );                                                                  //error; DSKx not mapped to a DOAD
            noExec();
          }
        }
        break;
    
        case 16:                                                                                          //MDSK(): map DSKx to DOAD
        case 17:                                                                                          //NDSK(): rename selected DOAD (DSK name is changed to the same)
        case 32:                                                                                          //RDSK(): remove DOAD
        case 33:                                                                                          //FGET(): get DOAD from FTP server
        case 34:                                                                                          //FPUT(): save selected DOAD to FTP server                                                                      
        {
          char DOADfullpath[20] = "\0";                                                                   //complete path + filename
          char DOADfilename[13] = "\0";                                                                   //just the filename for the FTP server
          strncpy( DOADfullpath, DSKfolder, 7 );                                                          //save current folder selection
          
          byte ii;
          for ( ii = 0; ii < 8; ii++ ) {                                                                  //max 8 characters for MSDOS filename
            byte cc = read_DSRAM( CALLBF + (ii + 2) );                                                    //read character from APEDSK99 CALL buffer
            if ( cc != 0x00 ) {                                                                           //end of filename (>00 added by DSR)? ...
              DOADfilename[ii] = cc;                                                                      //... no; add next character to filename
            } else {                                                                      
              break;                                                                                      // ... yes we need to get out
            }
          }                                                                           

          clrCALLbuffer();                                                                                //clear CALL buffer
          DOADfilename[ii] = '\0';                                                                        //properly terminate filename
          strncat( DOADfilename, ".DSK", 4 );                                                             //add file extenstion to filename
          strncat( DOADfullpath, DOADfilename, ++ii + 4 );                                                //add filename to path  

          for ( byte ii = 0; ii < 20; ii++ ) {
            write_DSRAM( 0x2000 + ii, DOADfullpath[ii] );
          }
        
          boolean existDOAD = SD.exists( DOADfullpath );                                                  //existing DOAD flag
          byte protectDOAD;                                                                               //Protected flag
          if ( existDOAD ) {                                                                              //does DOAD exist?
            DSKx = SD.open( DOADfullpath, FILE_READ);                                                     //yes; open DOAD file
            DSKx.seek(0x10);                                                                              //byte 0x10 in Volume Information Block stores status
            protectDOAD = DSKx.read();                                                                    //store Protected flag
          }

          if ( currentA99cmd == 16 ) {                                                                    //MDSK: map a DOAD to DSK1-3
            if ( existDOAD ) {                                                                            //does DOAD exist? ...
              if ( !getDSKparms( currentDSK ) ) {                                                         //check DOAD size and if OK store DSK parameters 
                strncpy( nameDSK[currentDSK], DOADfullpath, ii + 11 );                                    //we're good; assign to requested DSKx   
                activeDSK[currentDSK] = true;                                                             //flag as active
                protectDSK[currentDSK] = protectDOAD;                                                     //set Protected status: 0x50 || "P", 0x20 || " "
              } else {
                CALLstatus( DOADTooBig );                                                                 //... flag error ...
              }           
            } else {
              CALLstatus( DOADNotFound );                                                                 //... no; return error flag
            }

          } else if ( currentA99cmd == 17 ) {                                                             //NDSK: rename a mapped DOAD
              if ( !existDOAD ) {                                                                         //does DOAD exist? ...
                if ( activeDSK[currentDSK] ) {                                                            //is the requested disk mapped to a DOAD? ...   
                  if ( protectDSK[currentDSK] == 0x20 ) {                                                 //is DOAD unProtected? ...
                    DSKx = SD.open( nameDSK[currentDSK], FILE_WRITE );                                    //yes; safe to rename
                    DSKx.seek ( 0x00 );                                                                   //go to start of file
                    for ( byte ii = 7; ii < 15; ii++ ) {                                                  //write new (max 8 characters) DSK name ...
                      if ( DOADfullpath[ii] != '.' ) {
                         DSKx.write( DOADfullpath[ii] );
                      } else {
                        break;
                      }
                    }
                    while ( DSKx.position() < 0x0A ) {                                                    //... and fill rest (max 10 characters) with spaces
                      DSKx.write( 0x20 );
                    }                  
                    DSKx.flush();                                                                         //make sure changes are saved to SD ...
                    DSKx.rename( DOADfullpath );                                                          //... before renaming the file
                    activeDSK[currentDSK] = false;                                                        //flag as inactive to lose old mapping
                  } else {
                    CALLstatus( Protected );                                                              //... no; can't rename a protected DOAD
                  }
                } else {
                  CALLstatus( DOADNotMapped );                                                            //... no; can't rename a non-mapped DOAD
                }
              } else {
              CALLstatus( DOADexists );                                                                   //... yes; can't rename to an existing filename
            }
              
          } else if ( currentA99cmd == 32 ) {                                                             //RDSK() ?                                                                                     
            if ( existDOAD ) {                                                                            //does DOAD exist? ...
              if ( protectDOAD == 0x20 ) {                                                                //yes; is DOAD unProtected? ...
                for ( byte ii= 0; ii < 3; ii++ ) {
                  if ( strcmp(DOADfullpath, nameDSK[ii]) == 0 ) {                                         //is DOAD currently mapped?
                    activeDSK[ii] = false;                                                                //yes; flag DSKx non-active
                  }
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
            
            if ( currentA99cmd == 34 ) {                                                                  //FPUT()?
              if ( existDOAD ) {                                                                          //yep; does DOAD exist ...
                DSKx.seek(0);                                                                             //go to start of file (we were at 0x10)
                ftp.store( DOADfilename, DSKx );                                                          //yep; send DOAD to ftp server
              } else {
                CALLstatus( DOADNotFound );                                                               //no; report error to CALL()
              }
            } else if ( currentA99cmd == 33 ) {                                                           //FGET()?
              if ( existDOAD && protectDOAD == 0x50) {                                                    //yep; is existing DOAD Protected?
                CALLstatus( Protected );                                                                  //yes; report error to CALL()
              } else {
                DSKx.close();                                                                             //DOAD exists so is currently open. Need to close ...
                DSKx = SD.open( "/_FT", FILE_WRITE );                                                     //... as we are opening a temporary FTP file
                ftp.retrieve( DOADfilename, DSKx );                                                       //get DOAD from FTP server
                if ( DSKx.size() != 0 ) {                                                                 //size > 0 = transfer successful? ...
                  if ( existDOAD ) {                                                                      //yes; does DOAD already exist?
                    SD.remove( DOADfullpath );                                                            //yes -> delete
                  }
                  DSKx.rename( DOADfullpath );                                                            //rename temp FTP file to proper DOAD
                } else {     
                  CALLstatus( DOADNotFound );                                                             //report transfer error (check ownership/group/rights on FTP server)
                }
              }    
            }   
            EtherStop();                                                                                  //stop Ethernet client
          }                                        
          noExec();                                                                                       //we're done
        }
        break;       

        case 35:                                                                                          //ADSR(): load / change default DSR file
        {                                                                                 
          char DSRtoload[13] = "\0";                                                                      //filename new DSR
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
            currentA99cmd = 51;                                                                           //valid DSR file; fall through to ARST() to activate
          }
        }
        case 51:                                                                                          //ARST(): reset current DSR    
        { 
          if ( currentA99cmd == 51 ) {                                                                    //seems double-up but is needed for ADSR fall-through
            TIgo();                                                                                       //release TI
            APEDSK99reset();                                                                              //reset APEDSK99
          }
        }
        break;

        case 36:                                                                                          //NDIR(): change working root folder
        {  
          char newfolder[8] = "/\0";                                                                      //prep with starting "/"
          for ( byte ii = 1; ii < 6; ii++ ) {                                                             //add CALL characters
            newfolder[ii] = read_DSRAM( CALLBF + (ii + 1) );
          }
          newfolder[6] = '/';                                                                             //add trailing "/"
          newfolder[7] = '\0';                                                                            //properly terminate string

          if ( SD.exists( newfolder ) ) {                                                                 //does folder exist? ...
            DSKfolder[0] = '\0';                                                                          //... yes; prep global folder variable
            strncpy( DSKfolder, newfolder, 8 );                                                           //copy new folder name
            DSKfolder[7] = '\0';                                                                          //properly terminate string
          } else {
            CALLstatus( DIRNotFound );                                                                    //... no; error                                                    
          }
          noExec;
        }
        break;

        case 48:                                                                                          //SDSK(): show mapped DOAD's + available folders 
        {
          if ( newA99cmd ) {
            currentDSK = 0;                                                                               //first run make sure we start with DSK1
            gii = 0;                                                                                      //keep track when to display SDISK info (1 row = 2x 16 char fields
          }
        
          char DOADfullpath[20];
          clrCALLbuffer();                                                                                //clear CALL buffer

          if ( gii < 6 ) {                                                                                //6 rows               
            if ( !(gii & B00000001) ) {                                                                   //even row?

              write_DSRAM( CALLBF    , '1' + currentDSK + TIBias );                                       //DSK# + TI BASIC bias
              write_DSRAM( CALLBF + 1, '=' + TIBias );            
                         
              if ( activeDSK[currentDSK] ) {                                                              //is the requested disk mapped to a DOAD? ...                                 

                strncpy( DOADfullpath, nameDSK[currentDSK], 20 );                                         //... yes; get current DOAD name         

                DSKx = SD.open( nameDSK[currentDSK], FILE_READ );                                         //open DOAD         
                readFATts();                                                                              //read FAT time/date
                converTD();                                                                               //convert to string
                for ( byte ii = 17; ii < 29; ii++ ) {
                  write_DSRAM( CALLBF + ii, TimeDateASC[ii-17] + TIBias );                                //write time/date string to CALL buffer for display without year ...
                }
                write_DSRAM( CALLBF+29, TimeDateASC[14] + TIBias);                                        //... as only space to display last 2 year digits
                write_DSRAM( CALLBF+30, TimeDateASC[15] + TIBias);  
   
              } else {
                strncpy( DOADfullpath, " <no map>          ", 20 );                                       //... no; indicate not mapped
              }
 
              for ( byte ii = 2; ii < 16; ii++ ) {                                  
                write_DSRAM( CALLBF + (ii), DOADfullpath[ii-1] + TIBias );                                //store mapping characters in CALL buffer
                if ( DOADfullpath[ii] == '.' ) {                                                          //if the next character is a "." we're done
                  break;
                }
              } 
            
            } else {                                                                                      //odd row

              if ( activeDSK[currentDSK] ) {                                                              //is the requested disk mapped to a DOAD? ... 
                                
                DSKx.seek( 0 );                                                                           //... yes; read/save DSK name characters in CALL buffer
                for ( byte ii = 2; ii < 12; ii++ ) {                                  
                  write_DSRAM( CALLBF + ii, DSKx.read() + TIBias );                                    
                }   

                unsigned int tsectors =                                                                   //determine total #sectors of current DSK
                                        ( read_DSRAM(DSKprm + ( currentDSK * 6)) * 256) +
                                        ( read_DSRAM(DSKprm + ((currentDSK * 6) + 1)) );
                
                byte maxbitmap = tsectors / 8;                                                            //determine size of the bitmap to scan (1, 2 or 4 * 45 bytes)                              
                unsigned int countOnes = 0;   
  
                DSKx.seek( 0x38 );                                                                        //1st Sector Bitmap byte
                for ( byte ii = 0; ii < maxbitmap - 1; ii++ ) {                                           //sum all set bits (=sectors used) for all Sector Bitmap bytes
                  byte bitmap = DSKx.read();
                  for ( byte jj = 0; jj < 8; jj++ ) {                                       
                    countOnes += ( bitmap >> jj ) & 0x01;                                                 //"on the One you hear what I'm sayin'"
                  }
                }

                char fusectors[5] = "\0";                                                                 //ASCII store
                write_DSRAM( CALLBF + 13, 'T' + TIBias );
                write_DSRAM( CALLBF + 14, '=' + TIBias );
                sprintf( fusectors, "%4u", tsectors );                                                    //convert number of total #sectors to string
                for ( byte ii = 15; ii < 19; ii++ ) { 
                  write_DSRAM( CALLBF + ii, fusectors[ii-15] + TIBias );                                  //store ASCII # in CALL buffer
                }

                fusectors[0] = '\0';                                                                      //re-use ASCII store
                write_DSRAM( CALLBF + 20, 'F' + TIBias );
                write_DSRAM( CALLBF + 21, '=' + TIBias );
                sprintf( fusectors, "%4u", (maxbitmap * 8) - countOnes );                                 //convert number of free sectors to string
                for ( byte ii = 22; ii < 26; ii++ ) {                                                     //store ASCII # in CALL buffer
                  write_DSRAM( CALLBF + ii, fusectors[ii-22] + TIBias );
                }
             
                if ( protectDSK[currentDSK] == 0x50 ) {
                  write_DSRAM( CALLBF + 27, 'P' + TIBias );                                               //indicate DOAD is Protected or ...
                } else {
                  write_DSRAM( CALLBF + 27, 'U' + TIBias );                                               //... Unprotected
                }
              }
  
              DSKx.close();                                                                               //close current DSK before possibly opening another DSK
              if ( currentDSK < 2 ) {                                                                     //must stay within 0-2 for noExec() to properly close current open DSK[]
              currentDSK++;                                                                               //next DSK  
              }
            }
            gii++;                                                                                        //next screen row
  
          } else {                                                                                        //done DSK1-3, now show available /folders

            clrCALLbuffer();                                                                              //clear CALL buffer
            
            if ( gii == 6 ) {                                                                             //first run of folder display?
              DSKx.close();                                                                               //close DSKx before ...
              SDdir = SD.open( "/" );                                                                     //... openening root folder
              gii++;                                                                                      //make sure the above is run only once
            } 

            if ( gii == 8 ) {                                                                            //has empty screen row been displayed? ...
          
              clrCALLbuffer();                                                                            //... yes; clear CALL buffer       
    
              boolean pfolder = false;                                                                    //flag == true when !file && 5 chars
              while ( pfolder == false ) {
                File tFile = SDdir.openNextFile();                                                        //next file entry
                if ( tFile ) {                                                                            //valid file entry ?
                  if ( tFile.isDirectory() ) {                                                            //is it a folder (!regular file)?
                    char foldername[13] = "\0";                                                           //ASCII store 
                    tFile.getName( foldername, 13 );                                                      //copy foldername ... 
                    if ( foldername[5] == 0x00 ) {                                                        //if 6th array character is empty ...
                      byte ii;                                                                            //...we have a valid folder
                      for ( ii = 0; ii < 5 ; ii++ ) {                                                     //write folder name to CALL buffer
                        write_DSRAM( CALLBF + ii, foldername[ii] + TIBias );
                      }     
                      write_DSRAM( CALLBF + ii, '/' + TIBias );                                           //end folder name with "/"
                      pfolder = true;                                                                     //processed valid folder, escape the while loop
                    }        
                  }
                } else {                                                                                  //last file entry processed
                  SDdir.close();                                                                          //close root folder
                  CALLstatus( More );                                                                     //signal end of display items
                  noExec();                                                                               //stop processing 
                  pfolder =  true;                                                                        //escape while loop
                }  
              }
            } else {
              gii++;                                                                                      //... no; let DSR display an empty row
            }
          }  
        }                                        
        break;       

        case 49:                                                                                          //SDIR(): Show DOAD's in current folder on SD
        {
          clrCALLbuffer();                                                                                //clear CALL buffer
          if ( newA99cmd ) {                                                                              //first run of SDIR()?
            SDdir = SD.open( DSKfolder );                                                                 //yes; open directory
          }        
                 
          File tFile = SDdir.openNextFile();                                                              //get next file
          if ( tFile && read_DSRAM(CALLST) ==  AllGood ) {                                                //valid file AND !ENTER from DSR?
            for ( byte ii = 0; ii < 7; ii++ ) {                                                           //yes; write current folder name
              write_DSRAM( CALLBF + ii, DSKfolder[ii + 1] + TIBias );
            }
            
            char DOADfilename[13] = "\0";                                                                 //ASCII store
            tFile.getName( DOADfilename, 13 );                                                            //copy filename ... 
            for ( byte ii = 6; ii < 14 ; ii++ ) {                                                         //... and write to buffer
              if ( DOADfilename[ii - 6] != '.' ) {
                write_DSRAM( CALLBF + ii, DOADfilename[ii - 6] + TIBias );
              } else {
                break;
              }    
            }  

            for ( byte ii=20; ii < 30; ii++ ) {                                  
              write_DSRAM( CALLBF + ii, tFile.read() + TIBias );                                          //read/save DSK name characters in CALL buffer
            }               
            
            tFile.seek( 0x12 );                                                                           //byte >12 in VIB stores #sides (>01 or >02)
            write_DSRAM( CALLBF +  15, tFile.read() == 0x01 ? '1' + TIBias : '2' + TIBias  );             //#sides; change to ASCII and add TI screen bias
            write_DSRAM( CALLBF + 16, tFile.read() == 0x01 ? 'S' + TIBias : 'D' + TIBias);                //byte >13 in VIB stores density (>01/SD or >02/DD) 
            tFile.seek( 0x11 );                                                                           //byte >11 in VIB stores #tracks
            char tracks[3];                                                                               //ASCII store
            sprintf( tracks, "%2u", tFile.read() );                                                       //convert #tracks to string
            write_DSRAM( CALLBF + 17, tracks[0] + TIBias );
            write_DSRAM( CALLBF + 18, tracks[1] + TIBias );
              
            tFile.close();                                                                                //prep for next file
          } else {                                                                                        //... no
            SDdir.close();
            CALLstatus( More );                                                                           //done all files
            noExec(); 
          }
        }
        break;    

        case 50:                                                                                          //AHLP()
        {
          
          clrCALLbuffer();                                                                                //clear CALL buffer
          if ( newA99cmd ) {                                                                              //first run make sure we start with 1st message
            gii = 0;
          }

          if ( gii < 28 && read_DSRAM(CALLST) ==  AllGood ) {                                             //18x 32char arrays
            for ( byte ii = 0; ii < 31; ii++ ) {
              write_DSRAM( CALLBF + ii, pgm_read_byte( &CALLhelp[gii][ii] ) + TIBias );                   //read character array from PROGMEM and write to CALL buffer
            }
            gii++;                                                                                        //next
          } else {
            CALLstatus( More );                                                                           //done all help messages
            noExec();
          }
        }
        break;

        case 52:                                                                                          //TIME()
        case 19:                                                                                          //DSR: update FAT DSK date/time
        {
          clrCALLbuffer();                                                                                //clear CALL buffer
          EtherStart();                                                                                   //start Ethernet client
          
          if ( getNTPdt() == FNTPConnect ) {                                                              //check if NTP is available
            CALLstatus( FNTPConnect );                                                                    //no; report error
          } else if ( currentA99cmd == 52 ) {                                                             //TIME()
            converTD();                                                                                   //get full time/date string         
            for ( byte ii = 0; ii < 16; ii++ ) {
              write_DSRAM( CALLBF + ii, TimeDateASC[ii] + TIBias );                                       //copy string to CALL buffer                                   
            }
            CALLstatus( NTPStamp );                                                                       //done with a one-liner 
          } else if ( currentA99cmd == 19 ) {                                                             //DSR DSK NTP update (format, write / save file)                                                                           
            currentDSK = ( read_DSRAM( CRUWRI ) >> 1 ) & B00000011;                                       //determine selected disk in DSR command   
            currentDSK--;                                                                                 //DSK1-3 -> DSK0-2 to reduce array sizes          
            if ( protectDSK[currentDSK] != 0x50 ) {                                                       //DSK protected?
              DSKx = SD.open( nameDSK[currentDSK], FILE_WRITE );
              writeFATts();                                                                               //no; update DOAD FAT date/time
            }  
          }
          EtherStop();                                                                                    //stop Ethernet client
          CALLstatus( AllGood );
          noExec(); 
        }
        break;

        case 53:                                                                                          //ALOW(): load real lowercase character set
        { 
          boolean cFile = true;                                                                           //assume SD card contains valid char definition file 
          if ( newA99cmd ) {   
            cFile = (DSKx = SD.open( "/tilcchar.bin", FILE_READ) );                                       //first run, try to open it
            gii = 0;                                                                                      //initialise global counter 
          }

          if ( cFile ) {                                                                                  //char file successfully opened? ...
            if ( gii < 23 ) {                                                                             //... yes; more character definition to go? ...
              DSKx.seek( gii * 32 );                                                                      //... yes; next 32byte chunk from file
              for ( byte ii = 0; ii < 32; ii++ ) {  
                write_DSRAM( CALLBF + ii, DSKx.read() );                                                  //DSR display routine to write them to VDP pattern table
              }
              gii++;                                                                                      //next lot
            } else {
              CALLstatus( More );                                                                         //... no; all done
              noExec();                                                                                   //end it all
            }
          } else {
            CALLstatus( DEFNotFound );                                                                    //... no; error: can't find || open char definition file
            noExec();                                                                                     //end it all
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

  TIgo();                                                                                                 //TI, the floor is yours
 
} //end loop() 
