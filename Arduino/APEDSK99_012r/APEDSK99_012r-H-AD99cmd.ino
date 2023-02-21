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
        
        case 3:                                                                                           //LDSK(): list files on selected DSK
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
              for ( byte ii = 1; ii < 11; ii++ ) {                                  
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
                unsigned long tpos = DSKx.position();                                                     //remember current file pointer as we detour to determine PRG type
                DSKx.seek( tpos + 15 );                                                                   //go to 1st PRG sector value
                unsigned long BorA = DSKx.read() + ( (DSKx.read() & 0x0F) * 256 );                        //construct first sector with U,M and N nibbles
                DSKx.seek( BorA * NRBYSECT );                                                             //go to sector ...
                BorA  = (DSKx.read() * 256) + DSKx.read();                                                //... and check:
                if ( (BorA > 0x0000) && (BorA < 0xFFFF) ) {                                               //"option" 5 program files have either >FFFF (more segments) or >0000 (last segment) 
                  strncpy( filetype, "BAS\0", 4);                                                         //most likely a BASIC program
                } else {
                  strncpy( filetype, "PRG\0", 4);                                                         //"option 5" program
                }
                DSKx.seek( tpos );                                                                        //return to initial file position
              } else {
                strncpy( filetype, " ? \0", 4);                                                           //expect the unexpected
              }
  
              DSKx.seek( DSKx.position() + 4 );                                                           //byte >11 stores record length
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
  
              for ( byte ii = 12; ii < 28; ii++ ) {                                  
                write_DSRAM( CALLBF + ii, filetype[ii-12] + TIBias );                                     //save file characteristics in CALL buffer for display
              }
       
              if ( protect == true ) {                                                                    //indicate if file is protected 
                write_DSRAM( CALLBF + 29, 'P' + TIBias);
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

            if ( !ftp.connect(ACFG.ftpserver, ACFG.ftpuser, ACFG.ftpass) ) {                              //if we can't connect to FTP server ...
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

        case 35:                                                                                          //ADSR(): load / change default APEDSK99 DSR file
        {                                                                                 
          char DSRtoload[13]  = "\0";                                                                     //filename new DSR
          for ( byte ii = 2; ii < 10; ii++ ) {                                                            //read file name from CALL buffer
            DSRtoload[ii - 2] = read_DSRAM( CALLBF + ii );                                                //get new DSR from CALL buffer
          }                                                          
          DSRtoload[8] = '\0';                                                                        
          strncat( DSRtoload, ".DSR", 4 );                                                                //complete filename

          clrCALLbuffer();                                                                                //clear CALL buffer

          if ( !SD.exists(DSRtoload) ) {                                                                  //does new DSR exist?
            CALLstatus( DSRNotFound );                                                                    //nope; error
            noExec();
            break;
          } else {
            EEPROM.put( EEPROM_DSR, DSRtoload );                                                          //no; write new DSR to EEPROM (only changes will be written)
          }
          currentA99cmd = 104;                                                                            //valid DSR file; fall through to ARST() to activate
        }

        case 104:                                                                                         //reset current DSR ( ADSR(), ARST() )  
        { 
          TIgo();                                                                                         //release TI
          APEDSK99reset();                                                                                //reset APEDSK99
        }
        break;

        //show version information before reset. Ideally should be run after reset but that would mean    //ARST() primary command: version info
        //TMS9900 code outside the DSR space as the latter is obviously reset ;-)
        case 52: 
        {
          CALLstatus( AInit );                                                                            //save version message body in CALL buffer
          for ( byte ii = 0; ii < 4; ii++ ) {                                                             
            write_DSRAM( CALLBF + (ii+14), read_DSRAM( AVersion + ii ) + TIBias );                        //add DSR version number
          }
          
        }
        break;

        case 36:                                                                                          //CDIR(): change working root folder
        {  
          char newfolder[8] = "/";                                                                        //prep with starting "/"
          byte ii;
          for ( ii = 1; ii < 7; ii++ ) {                                                                  //max 5 characters for folder name
            byte cc = read_DSRAM( CALLBF + (ii + 1) );                                                    //read character from APEDSK99 CALL buffer
            if ( cc != 0x00 ) {                                                                           //end of filename (>00 added by DSR)? ...
              newfolder[ii] = cc;                                                                         //... no; add next character to folder name
            } else {                                                                      
              break;                                                                                      // ... yes we need to get out
            }
          }        
          
          newfolder[ii] = '/';                                                                            //add trailing "/"
          newfolder[ii + 1] = '\0';                                                                       //properly terminate string

          clrCALLbuffer();                                                                                //clear CALL buffer

          if ( SD.exists( newfolder ) ) {                                                                 //does folder exist? ...
            DSKfolder[0] = '\0';                                                                          //... yes; prep global folder variable
            strncpy( DSKfolder, newfolder, 7 );                                                           //copy new folder name
            DSKfolder[7] = '\0';                                                                          //properly terminate string
            CALLstatus( AllGood );                                                                        //done
          } else {
            CALLstatus( DIRNotFound );                                                                    //... no; error                                                    
          }          
          noExec();
        }
        break;

        case 48:                                                                                          //SMAP(): show mapped DOAD's + available folders 
        {
          if ( newA99cmd ) {
            currentDSK = 0;                                                                               //first run make sure we start with DSK1
            gii = 0;                                                                                      //keep track when to display SDISK info (1 row = 2x 16 char fields
          }
        
          char DOADfullpath[20];
          clrCALLbuffer();                                                                                //clear CALL buffer

          if ( gii < 8 ) {                                                                                //8 rows               
            if ( (gii == 0) || (gii == 3) || (gii == 6) ) {                                               //selected rows only

              write_DSRAM( CALLBF + 1   , '1' + currentDSK + TIBias );                                    //DSK# + TI BASIC bias (leading space)
              write_DSRAM( CALLBF + 2, '=' + TIBias );            
                         
              if ( activeDSK[currentDSK] ) {                                                              //is the requested disk mapped to a DOAD? ...                                 

                strncpy( DOADfullpath, nameDSK[currentDSK], 20 );                                         //... yes; get current DOAD name         

                DSKx = SD.open( nameDSK[currentDSK], FILE_READ );                                         //open DOAD   
                DSKx.seek( 0 );                                                                           //... yes; read/save DSK name characters in CALL buffer
                for ( byte ii = 18; ii < 28; ii++ ) {                                  
                  write_DSRAM( CALLBF + ii, DSKx.read() + TIBias );                                    
                }   
   
                if ( protectDSK[currentDSK] == 0x50 ) {
                  write_DSRAM( CALLBF + 30, 'p' + TIBias );                                               //indicate DOAD is Protected or ...
                } else {
                  write_DSRAM( CALLBF + 30, 'u' + TIBias );                                               //... Unprotected
                }             
              
              } else {
                strncpy( DOADfullpath, " <no map>          ", 20 );                                       //... no; indicate not mapped
              }
 
              for ( byte ii = 3; ii < 17; ii++ ) {                                
                write_DSRAM( CALLBF + (ii), DOADfullpath[ii-2] + TIBias );                                //store mapping characters in CALL buffer
                if ( DOADfullpath[ii - 1] == '.' ) {                                                      //if the next character is a "." we're done
                  break;
                }
              }      
            
            } else if ( (gii == 1) || (gii == 4) || (gii == 7) ) {                                        //selected rows only

              if ( activeDSK[currentDSK] ) {                                                              //is the requested disk mapped to a DOAD? ... 
               
                readFATts();                                                                              //read FAT time/date
                converTD();                                                                               //convert to string
                for ( byte ii = 3; ii < 15; ii++ ) {
                  write_DSRAM( CALLBF + ii, TimeDateASC[ii-3] + TIBias );                                //write time/date string to CALL buffer for display without year ...
                }
                write_DSRAM( CALLBF+15, TimeDateASC[14] + TIBias);                                        //... as only space to display last 2 year digits
                write_DSRAM( CALLBF+16, TimeDateASC[15] + TIBias);                                         

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
                write_DSRAM( CALLBF + 18, 't' + TIBias );
                write_DSRAM( CALLBF + 19, '=' + TIBias );
                sprintf( fusectors, "%4u", tsectors );                                                    //convert number of total #sectors to string
                for ( byte ii = 20; ii < 24; ii++ ) { 
                  write_DSRAM( CALLBF + ii, fusectors[ii-20] + TIBias );                                  //store ASCII # in CALL buffer
                }

                fusectors[0] = '\0';                                                                      //re-use ASCII store
                write_DSRAM( CALLBF + 25, 'f' + TIBias );
                write_DSRAM( CALLBF + 26, '=' + TIBias );
                sprintf( fusectors, "%4u", (maxbitmap * 8) - countOnes );                                 //convert number of free sectors to string
                for ( byte ii = 27; ii < 31; ii++ ) {                                                     //store ASCII # in CALL buffer
                  write_DSRAM( CALLBF + ii, fusectors[ii-27] + TIBias );
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
            
            if ( gii == 8 ) {                                                                             //first run of folder display?
              DSKx.close();                                                                               //close DSKx before ...
              SDdir = SD.open( "/" );                                                                     //... openening root folder
              gii++;                                                                                      //make sure the above is run only once
            } 

            if ( gii == 10 ) {                                                                            //has empty screen row been displayed? ...
          
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
                      for ( ii = 1; ii < 6 ; ii++ ) {                                                     //write folder name to CALL buffer
                        byte cc = foldername[ii - 1];
                        if ( cc != 0x00 ) {                                                               //could be <5 characters
                          write_DSRAM( CALLBF + ii, cc + TIBias );
                        } else {
                          break;
                        }
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

        case 49:                                                                                          //LDIR(): list DOAD's in current folder on SD
        {
          clrCALLbuffer();                                                                                //clear CALL buffer
          if ( newA99cmd ) {                                                                              //first run of LDIR()?
            SDdir = SD.open( DSKfolder );                                                                 //yes; open directory
            gii=0;
            byte ii;
            for ( ii = 1; ii < 7; ii++ ) {                                                                //yes; write current folder name
              byte cc = DSKfolder[ii];
              if ( cc != 0x00 ) {                                                                         //could be <5 characters
                write_DSRAM( CALLBF + ii, cc + TIBias );
              } else {
                break;
              }
            }
            write_DSRAM( CALLBF + ii, ':' + TIBias );
          }    

          if ( gii > 2 ) {
            File tFile = SDdir.openNextFile();                                                            //get next file
            if ( tFile && read_DSRAM(CALLST) ==  AllGood ) {                                              //valid file AND !ENTER from DSR?            
              
              char DOADfilename[13] = "\0";                                                               //ASCII store
              tFile.getName( DOADfilename, 13 );                                                          //copy filename ... 
              for ( byte ii = 1; ii < 9 ; ii++ ) {                                                        //... and write to buffer
                if ( DOADfilename[ii - 1] != '.' ) {
                  write_DSRAM( CALLBF + ii, DOADfilename[ii - 1] + TIBias );
                } else {
                  break;
                }    
              }  
  
              for ( byte ii=11; ii < 21; ii++ ) {                                  
                write_DSRAM( CALLBF + ii, tFile.read() + TIBias );                                        //read/save DSK name characters in CALL buffer
              }               
              
              tFile.seek( 0x12 );                                                                         //byte >12 in VIB stores #sides (>01 or >02)
              write_DSRAM( CALLBF + 22, tFile.read() == 0x01 ? '1' + TIBias : '2' + TIBias );             //#sides; change to ASCII and add TI screen bias
              write_DSRAM( CALLBF + 23, 'S' + TIBias );
              write_DSRAM( CALLBF + 24, '/' + TIBias );   
              write_DSRAM( CALLBF + 25, tFile.read() == 0x01 ? 'S' + TIBias : 'D' + TIBias );             //byte >13 in VIB stores density (>01/SD or >02/DD) 
              write_DSRAM( CALLBF + 26, 'D' + TIBias );
              write_DSRAM( CALLBF + 27, '/' + TIBias );  
              tFile.seek( 0x11 );                                                                         //byte >11 in VIB stores #tracks
              char tracks[3];                                                                             //ASCII store
              sprintf( tracks, "%2u", tFile.read() );                                                     //convert #tracks to string
              write_DSRAM( CALLBF + 28, tracks[0] + TIBias );
              write_DSRAM( CALLBF + 29, tracks[1] + TIBias );
              write_DSRAM( CALLBF + 30, 'T' + TIBias );  
                
              tFile.close();                                                                              //prep for next file
            } else {                                                                                      //... no
              SDdir.close();
              CALLstatus( More );                                                                         //done all files
              noExec(); 
            }
          }
          gii++;
        }
        break;    

        case 50:                                                                                          //AHLP()
        {
          if ( newA99cmd ) {                                                                              //first run make sure we start with 1st message
            clrCALLbuffer();                                                                              //clear CALL buffer
            DSKx = SD.open( "/CALLhlp.txt", FILE_READ );                                                  //first run, open help file
            gii = 0;
          }

          if ( gii < 18 && read_DSRAM(CALLST) ==  AllGood ) {                                             //18 rows x 29 characters
            for ( byte ii = 1; ii < 30; ii++ ) {
              write_DSRAM( CALLBF + ii, DSKx.read() + TIBias );                                           //copy help text to CALL buffer
            }  
            gii++;                                                                                        //next
          } else {
            CALLstatus( More );                                                                           //done all help messages
            noExec();
          }
        }
        break;
     
        case 51:                                                                                          //ACHR(): load real lowercase character set
        { 
          if ( newA99cmd ) {   
            DSKx = SD.open( "/tilcchar.bin", FILE_READ );                                                 //first run, open definition file
            gii = 0;                                                                                      //initialise global counter 
          }

          if ( gii < 23 ) {                                                                               //more definitions to go? ...
            for ( byte ii = 0; ii < 32; ii++ ) {                                                          //... yes; next 32byte chunk from file
              write_DSRAM( CALLBF + ii, DSKx.read() );                                                    //DSR display routine to write them to VDP pattern table
            }
            gii++;                                                                                        //next lot
          } else {
            CALLstatus( More );                                                                           //... no; all done
            noExec();                                                                                     //end it all
          }
        }
        break;   

        case 53:                                                                                          //TIME()
        case 19:                                                                                          //DSR: update FAT DSK date/time
        {
          clrCALLbuffer();                                                                                //clear CALL buffer
          EtherStart();                                                                                   //start Ethernet client
          
          if ( getNTPdt() == FNTPConnect ) {                                                              //check if NTP is available
            CALLstatus( FNTPConnect );                                                                    //no; report error
          } else if ( currentA99cmd == 53 ) {                                                             //TIME()
            converTD();                                                                                   //get full time/date string         
            for ( byte ii = 0; ii < 16; ii++ ) {
              write_DSRAM( CALLBF + ii, TimeDateASC[ii] + TIBias );                                       //copy string to CALL buffer                                   
            }
            CALLstatus( AllGood );                                                                        //done
          } else if ( currentA99cmd == 19 ) {                                                             //DSR DSK NTP update (format, write / save file)                                                                           
            currentDSK = ( read_DSRAM( CRUWRI ) >> 1 ) & B00000011;                                       //determine selected disk in DSR command   
            currentDSK--;                                                                                 //DSK1-3 -> DSK0-2 to reduce array sizes          
            if ( protectDSK[currentDSK] != 0x50 ) {                                                       //DSK protected?
              DSKx = SD.open( nameDSK[currentDSK], FILE_WRITE );
              writeFATts();                                                                               //no; update DOAD FAT date/time
            }  
            CALLstatus( AllGood );                                                                        //done
          }
          EtherStop();                                                                                    //stop Ethernet client
          noExec(); 
        }
        break;
        
        case  54:                                                                                         //ACFG(): configure network
        {  
          DSKx = SD.open( nameDSK[0], FILE_READ );                                                        //expect ACFG.DSK mapped to DSK1
          DSKx.seek( 0 );                                                                                 //TI DSK name location

          byte ii;                                                                  
          char tStr[] = "ACFG";                                                                           //TI DSK name should be this                                                             
          for ( ii = 0; ii < 4; ii++ ) {                                                                  //check if so
            if ( DSKx.read() != tStr[ii] ) {
              break;
            }
          }
         
          if ( ii < 3 ) {                                                                                 //not all characters matched so ...
            CALLstatus( DOADNotMapped );                                                                  //... no genuine ACFG DOAD mapped at DSK1
            break;  
          } else {
            
            DSKx.seek( 0x21FF );                                                                          //ok genuine enough, go to config data

            byte cc = 0;                                                                                  //reset cc
            byte octet;
            for ( byte ii = 0; ii < 8; ii++ ) {                                                           //8 octets in total
              while ( cc != 0x08 ) {                                                                      //check for 0x08 delimiter
                cc = DSKx.read();
              }
              cc = ( DSKx.read() & B00000011 );                                                           //check for 0x4x
              if ( cc  == 0 ) {                                                                           //x=0 so octet[0-16]
                octet = DSKx.read();                                                                      //store octet
              } else {
                octet = cc * (DSKx.read() * 100) + DSKx.read();                                           //octet = x * (next byte * 100) + next byte
              } 
              if ( ii < 4 ) {                                                                             //first 4 values are the Ethernet Shield IP address ...
               ACFG.ipaddress[ii] = octet;
              } else {
                ACFG.ftpserver[ii-4] = octet;                                                             //... second 4 values are the FTP server address
              }
            }
 
            cc = 0;                                                                                       //reset cc to ...
            while ( cc == 0x00 ) {                                                                        //... check for next delimiter (string length)
              cc = DSKx.read();
            }
            for ( byte ii = 0; ii < cc; ii ++ ) {                                                         //store NTP IP address string
              ACFG.ntpserver[ii] = DSKx.read();
            } 
            ACFG.ntpserver[cc] = '\0';                                                                    //NTP server string delimiter        

            cc = DSKx.read();                                                                             //read string length
            for ( byte ii = 0; ii < cc; ii++ ) {                                                          //store FTP user name; cc has #characters
              ACFG.ftpuser[ii] = DSKx.read();
            }
            ACFG.ftpuser[cc] = '\0';                                                                      //FTP username string delimiter
            
            cc = DSKx.read();                                                                             //read string length
            for ( byte ii = 0; ii < cc; ii++ ) {                                                          //store FTP password; cc has #characters
              ACFG.ftpass[ii] = DSKx.read();
            }  
            ACFG.ftpass[cc] = '\0';                                                                       //FTP password string delimiter
            
            DSKx.seek( DSKx.position() + 2 );                                                             //skip 0x4x (not used for 0-15 values)   
            ACFG.TZ = DSKx.read();                                                                        //store timezone value

            EEPROM.put( EEPROM_CFG,  ACFG );                                                              //save configuration struct to EEPROM

            noExec();                                                                                     //done
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
