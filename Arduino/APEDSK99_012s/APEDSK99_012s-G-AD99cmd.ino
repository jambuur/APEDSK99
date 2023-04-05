    //***** APEDSK99-specfic commands
    
    currentA99cmd = read_DSRAM( ACOMND );
    if ( currentA99cmd != 0 ) {
      
      if ( newA99cmd = (currentA99cmd != lastA99cmd) ) {                                                  //new or continue previous APEDSK99 command?
        lastA99cmd = currentA99cmd;                                                                       //new; remember new command for next compare
        currentDSK = read_DSRAM( CALLBF );                                                                //read target DSKx
        if ( currentDSK > 0 ) {
          --currentDSK;                                                                                   //DSK1-3 -> DSK0-2 to reduce array sizes
        }
        CALLstatus( AllGood );                                                                            //start with clean execution slate
      }

      switch ( currentA99cmd ) {
        
        case  1:                                                                                          //PDSK: set DSK protection    
        case  2:                                                                                          //UDSK: reset DSK protection
        {
          if ( activeDSK[currentDSK] ) {                                                                  //is the requested disk mapped to a DOAD? ...
            protectDSK[currentDSK] = ( currentA99cmd == 1 ? 0x50 : 0x20 );                                //yes; PDSK or UDSK?      
            DSKx = SD.open( nameDSK[currentDSK], FILE_WRITE);                                             //open DOAD file to change write protect status 
            DSKx.seek(0x10);                                                                              //byte 0x10 in Volume Information Block stores 
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
              DSKx.seek( long(FDR) * NRBYSECT );                                                          //locate FDR within DOAD
              writeCBuffer( 1, 11, "\0", 0, true, false, '\0' );                                          //read/save filename characters in CALL buffer
            
              DSKx.seek( DSKx.position() + 2 );                                                           //byte >0C: file type 
              byte ftype = DSKx.read();
              boolean protect = (ftype & 0x08) != 0;                                                      //bit 4 set means file is protected

              ftype &= 0x83;                                                                              //strip protection bit
              if ( ftype != 1 ) {                                                                         //PROGRAM?
                write_DSRAM( CALLBF + 12, ( ftype & 0x0F ? 'I' : 'D') + TIBias );                         //DISPLAY (0) or INTERNAL (2)?
                write_DSRAM( CALLBF + 13, '/' + TIBias );                                                 //"/" as in "D/V"
                write_DSRAM( CALLBF + 14, ( ftype & 0xF0 ? 'V' : 'F') + TIBias );                         //VARIABLE (128) or FIXED (0)?
                 
                DSKx.seek( DSKx.position() + 4 );                                                         //byte >11 stores record length
                rJust( 15, 18, DSKx.read() );                                                             //write right-justified record length to CALL buffer
                DSKx.seek( DSKx.position() - 4 );                                                         //re-position to bytes >0E, >0F: #sectors
              } else {  
                writeCBuffer( 12, 15, "PRG", -12, false, false, '\0' );                                   //must be "PRG"; write to CALL buffer
                DSKx.seek( DSKx.position() + 1 );                                                         //re-position to bytes >0E, >0F: #sectors
              }   

              long tSectors = ( DSKx.read() * 256) + (DSKx.read() + 1 );                                  //read # of sectors used by this file
              rJust( 20, 23, tSectors );                                                                  //right-justified #sectors in CALL buffer
              rJust( 24, 29, ( tSectors * 256 ) - ( 256 - DSKx.read() ) );                                //right-justified equivalent in bytes (- unused #bytes in last sector) to CALL buffer

              if ( protect == true ) {                                                                    //indicate if file is protected 
                write_DSRAM( CALLBF + 30, 'p' + TIBias);
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
        {            
          if ( prepDSK() ) {                                                                              //does DOAD exist? ...
            if ( !getDSKparms( currentDSK ) ) {                                                           //check DOAD size and if OK store DSK parameters 
              memset( nameDSK[currentDSK], 0x00, 22 );                                                    //clear current DSK innfo
              Acatpy( nameDSK[currentDSK], sizeof(nameDSK[currentDSK]),                                   //assign MDSK info to requested DSKx   
                      DOADfullpath, sizeof(DOADfullpath) );  
              activeDSK[currentDSK] = true;                                                               //flag as active
              protectDSK[currentDSK] = protectDOAD;                                                       //set Protected status: 0x50 || "P", 0x20 || " "
            } else {
              CALLstatus( DOADTooBig );                                                                   //... flag error ...
            }           
          } else {
            CALLstatus( DOADNotFound );                                                                   //... no; return error flag
          } 
          noExec();
        }
        break;
        
        case 17:                                                                                          //NDSK(): rename selected DOAD (TI DSK name is changed to the same)
        {
          if ( !prepDSK() ) {                                                                             //does DOAD exist? ...
            if ( activeDSK[currentDSK] ) {                                                                //is the requested disk mapped to a DOAD? ...   
              if ( protectDSK[currentDSK] == 0x20 ) {                                                     //is DOAD unProtected? ...
                DSKx = SD.open( nameDSK[currentDSK], FILE_WRITE );                                        //yes; safe to rename
                DSKx.seek ( 0x00 );                                                                       //go to start of file

                byte ii = 0;
                while ( ii < 8 && DOADfilename[ii] != '.' ) {
                  DSKx.write( DOADfilename[ii++] );                                                       //write new DSK name (max 8 characters || until we hit ".")
                }                     
                while ( DSKx.position() < 0x0A ) {                                                        //... and fill rest (max 10 TI DSK characters) with spaces
                  DSKx.write( 0x20 );
                }                  

                DSKx.flush();                                                                             //make sure changes are saved to SD ...
                DSKx.rename( DOADfullpath );                                                              //... before renaming the file
                activeDSK[currentDSK] = false;                                                            //flag as inactive to lose old mapping
              } else {
                CALLstatus( Protected );                                                                  //... no; can't rename a protected DOAD
              }
            } else {
              CALLstatus( DOADNotMapped );                                                                //... no; can't rename a non-mapped DOAD
            }
          } else {
            CALLstatus( DOADexists );                                                                     //... yes; can't rename to an existing filename
          }
          noExec();
        }
        break;
        
        case 32:                                                                                          //RDSK(): remove DSK
        {
          if ( prepDSK() ) {                                                                              //does DOAD exist? ...
            if ( protectDOAD == 0x20 ) {                                                                  //yes; is DOAD unProtected? ...
              for ( byte ii = 0; ii < 3; ++ii ) {                                                         //yes ...
                if ( Acomp(DOADfullpath, sizeof(DOADfullpath), nameDSK[ii]) ) {                           //... check if DSK is already mounted ...
                  activeDSK[ii] = false;                                                                  //... flag DSKx non-active 
                }
              }
              SD.remove( DOADfullpath );                                                                  //yes; delete DOAD from SD card  
            } else {
              CALLstatus( Protected );                                                                    //... no; Protected, report error to CALL()
            }
          } else {
            CALLstatus( DOADNotFound );                                                                   //... no; return error flag
          }
          noExec();
        }
        break;
        
        case 33:                                                                                          //FGET(): get DOAD from FTP server
        case 34:                                                                                          //FPUT(): save selected DOAD to FTP server                                                                      
        {
          EtherStart();                                                                                   //start Ethernet client

          if ( !ftp.connect(ACFG.ftpserver, ACFG.ftpuser, ACFG.ftpass) ) {                                //if we can't connect to FTP server ...
            CALLstatus( FNTPConnect );                                                                    //... flag error ...     
            currentA99cmd = 0;                                                                            //... and cancel further command execution
          }

          boolean existDOAD = prepDSK();                                                                  //check if DOAD exists and save Protected status
            
          if ( currentA99cmd == 34 ) {                                                                    //FPUT()?
            if ( existDOAD ) {                                                                            //yep; does DOAD exist ...
              DSKx.seek(0);                                                                               //... yes go to start of file (we were at 0x10)                        
              ftp.store( DOADfilename, DSKx );                                                            //send DOAD to ftp server
            } else { 
              CALLstatus( DOADNotFound );                                                                 //no; report error to CALL()
            } 
          } else if ( currentA99cmd == 33 ) {                                                             //FGET()
            if ( existDOAD && protectDOAD == 0x50) {                                                      //is existing DOAD Protected?
              CALLstatus( Protected );                                                                    //yes; report error to CALL()
            } else {
              DSKx.close();                                                                               //DOAD exists so is currently open. Need to close ...                
              DSKx = SD.open( "/_FT", FILE_WRITE );                                                       //... as we are opening a temporary FTP file
              ftp.retrieve( DOADfilename, DSKx );                                                         //get DOAD from FTP server
              if ( DSKx.size() != 0 ) {                                                                   //size > 0 = transfer successful? ...
                if ( existDOAD ) {                                                                        //yes; does DOAD already exist?
                  SD.remove( DOADfullpath );                                                              //yes -> delete
                }
                DSKx.rename( DOADfullpath );                                                              //rename temp FTP file to proper DOAD
              } else {     
                CALLstatus( DOADNotFound );                                                               //report transfer error (check ownership/group/rights on FTP server)
              }
            }  
          }  
          EtherStop();                                                                                    //stop Ethernet client
          noExec();                                                                                       //we're done
        }
        break;       

        case 35:                                                                                          //ADSR(): load / change default APEDSK99 DSR file
        {                                                                                       
          readCBuffer();                                                                                  //get filename  
          Acatpy( DOADfilename, sizeof(DOADfilename), ".DSR", 4);                                         //stick extension at the back

          if ( SD.exists(DOADfilename) ) {                                                                //does new DSR exist?
            EEPROM.put( EEPROM_DSR, DOADfilename );                                                       //yes; update DSR info in EEPROM ...
            currentA99cmd = 104;                                                                          //... and fall through to ARST() to activate
          } else {
            CALLstatus( DSRNotFound );                                                                    //nope; error
            noExec();
            break;
          }
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
          for ( byte ii = 0; ii < 4; ++ii ) {                                                             
            write_DSRAM( CALLBF + (ii+14), read_DSRAM( AVersion + ii ) + TIBias );                        //add DSR version number
          }
          
        }
        break;

        case 36:                                                                                          //CDIR(): change working root folder
        {           
          byte nPos = readCBuffer();                                                                      //get folder name
          DOADfilename[nPos] = '/';

          if ( SD.exists( DOADfilename ) ) {                                                              //does folder exist? ...
            memset( DSKfolder, 0x00, 10 );                                                                 //... yes; clear global folder variable ...
            Acatpy( DSKfolder, sizeof(DSKfolder), DOADfilename, sizeof(DOADfilename) );                   //... and copy new folder name
            CALLstatus( AllGood );                                                                        //done
          } else {
            CALLstatus( DIRNotFound );                                                                    //... no; error                                                    
          }          
          noExec();                                                                                       //all done
        }
        break;

        case 48:                                                                                          //SMAP(): show mapped DOAD's + available folders 
        {
          if ( newA99cmd ) {
            currentDSK = 0;                                                                               //first run make sure we start with DSK1
            gii = 0;                                                                                      //keep track when to display SDISK info (1 row = 2x 16 char fields
          }
        
          clrCALLbuffer();                                                                                //clear CALL buffer

          if ( gii < 8 ) {                                                                                //8 rows               
            if ( (gii == 0) || (gii == 3) || (gii == 6) ) {                                               //selected rows only

              write_DSRAM( CALLBF + 1, '1' + currentDSK + TIBias );                                       //DSK# + TI BASIC bias (leading space)
              write_DSRAM( CALLBF + 2, '=' + TIBias );            
                         
              if ( activeDSK[currentDSK] ) {                                                              //is the requested disk mapped to a DOAD? ...                                 

                DSKx = SD.open( nameDSK[currentDSK], FILE_READ );                                         //open DOAD   
                DSKx.seek( 0 );                                                                           //go to start of DSK               
                writeCBuffer( 18, 28, "\0", 0, true, false, '\0' );                                       //read/save DSK name characters in CALL buffer
                            
                if ( protectDSK[currentDSK] == 0x50 ) {
                  write_DSRAM( CALLBF + 30, 'p' + TIBias );                                               //indicate DOAD is Protected
                }      
                                   
                writeCBuffer( 3, 17, nameDSK[currentDSK], -3, false, true, '.' );                         //store mapping characters in CALL buffer

              } else {
                writeCBuffer( 3, 12, "<no map>", -3, false, false, '\0' );                                //DSK not mapped
              }
           
            } else if ( (gii == 1) || (gii == 4) || (gii == 7) ) {                                        //selected rows only

              if ( activeDSK[currentDSK] ) {                                                              //is the requested disk mapped to a DOAD? ... 
               
                readFATts();                                                                              //read FAT time/date
                converTD( 3, true );                                                                      //convert to string (2-digit year) and store in CALL buffer starting at x
                
                unsigned int tSectors =                                                                   //determine total #sectors of current DSK
                                        ( read_DSRAM(DSKprm + ( currentDSK * 6)) * 256) +
                                        ( read_DSRAM(DSKprm + ((currentDSK * 6) + 1)) );
                
                byte maxbitmap = tSectors / 8;                                                            //determine size of the bitmap to scan (1, 2 or 4 * 45 bytes)                              
                unsigned int countOnes = 0;   
  
                DSKx.seek( 0x38 );                                                                        //1st Sector Bitmap byte
                for ( byte ii = 0; ii < maxbitmap - 1; ++ii ) {                                           //sum all set bits (=sectors used) for all Sector Bitmap bytes
                  byte bitmap = DSKx.read();
                  for ( byte jj = 0; jj < 8; jj++ ) {                                       
                    countOnes += ( bitmap >> jj ) & 0x01;                                                 //"on the One you hear what I'm sayin'"
                  }
                }
                
                writeCBuffer( 18, 27, "t=     f=", -18, false, false, '\0' );
                rJust( 20, 24, tSectors );                                                                //right-justified #sectors in CALL buffer
                rJust( 27, 31, (maxbitmap * 8) - countOnes );                                             //right-justified free sectors in CALL buffer   
              }
  
              DSKx.close();                                                                               //close current DSK before possibly opening another DSK
              if ( currentDSK < 2 ) {                                                                     //must stay within 0-2 for noExec() to properly close current open DSK[]
              currentDSK++;                                                                               //next DSK  
              }
            }
            gii++;                                                                                        //next screen row
  
          } else {                                                                                        //done DSK1-3, now show available /folders

            clrCALLbuffer();                                                                              //print row of spaces
            
            if ( gii == 8 ) {                                                                             //first run of folder display?
              DSKx.close();                                                                               //close DSKx before ...
              SDdir = SD.open( "/" );                                                                     //... openening root folder
              gii++;                                                                                      //make sure the above is run only once
            } 

            if ( gii == 10 ) {                                                                            //has empty screen row been displayed? ...
          
              clrCALLbuffer();                                                                            //... yes; clear CALL buffer       
    
              boolean pfolder = false;                                                                    //flag == true when !file && 5 chars
              byte nPos;
              while ( pfolder == false ) {
                DSKx = SDdir.openNextFile();                                                              //next file entry
                if ( DSKx ) {                                                                             //valid file entry ?
                  if ( DSKx.isDirectory() ) {                                                             //is it a folder (!regular file)?
                    memset( DOADfilename, 0x00, 13 );                                                     //clear for next entry
                    DSKx.getName( DOADfilename, 13 );                                                     //copy foldername ... 
                    if ( !Acomp(DOADfilename, 9, "SYSTEM~1") ) {                                          //skip SD system folder
                      nPos  = writeCBuffer( 1, 9, DOADfilename, -1, false, true, 0x00 );                  //... we have a valid folder
                      write_DSRAM( CALLBF + nPos, '/' + TIBias );                                         //end folder name with "/"
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
            byte nPos  = writeCBuffer( 1, 9, DSKfolder, -1, false, true, 0x00 );                           //yes; write current folder name to CALL buffer
            write_DSRAM( CALLBF + nPos, ':' + TIBias );
          }    

          if ( gii > 2 ) {
            DSKx = SDdir.openNextFile();                                                                  //get next file
            if ( DSKx && read_DSRAM(CALLST) ==  AllGood ) {                                               //valid file AND !ENTER from DSR?
              memset( DOADfilename, 0x00, 13 );                                                           //clear for next entry
              DSKx.getName( DOADfilename, 13 );                                                           //copy filename ... 
              writeCBuffer( 1, 9, DOADfilename, -1, false, true, '.' );                                   //... and write to CALL buffer
              writeCBuffer( 11, 21, "\0", 0, true, false, '\0' );                                         //write TI DSK name to CALL buffer   

              DSKx.seek(0x10); 
              if ( DSKx.read() == 0x50 ) {
                write_DSRAM( CALLBF + 31, 'p' + TIBias );                                                 //byte >10 in VIB stores density (Un)Protected status
              }  
              rJust( 28, 30, DSKx.read() );                                                               //byte >11 in VIB stores #tracks
              write_DSRAM( CALLBF + 22, (DSKx.read() == 0x01 ? '1' : '2') + TIBias );                     //byte >12 in VIB stores #sides (>01 or >02)
              writeCBuffer( 23, 28, "S/ D/", -23, false, false, '\0' );
              write_DSRAM( CALLBF + 25, (DSKx.read() == 0x01 ? 'S' : 'D') + TIBias );                     //byte >13 in VIB stores density (>01/SD or >02/DD) 

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
            writeCBuffer( 1, 30, "\0", 0, true, false, '\0' );
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
            writeCBuffer( 0, 32, "\0", 0, true, false, '\0' );                                            //... yes, write next chunk of 32 definition bytes to CALL buffer
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
            converTD( 0, false );                                                                         //NTP date/time string (4 digit year) to CALL buffer
            CALLstatus( AllGood );                                                                        //done
          } else if ( currentA99cmd == 19 ) {                                                             //DSR DSK NTP update (format, write / save file)                                                                           
            currentDSK = ( read_DSRAM( CRUWRI ) >> 1 ) & B00000011;                                       //determine selected disk in DSR command   
            currentDSK--;                                                                                 //DSK1-3 -> DSK0-2 to reduce array sizes          
            --currentDSK;                                                                                 //DSK1-3 -> DSK0-2 to reduce array sizes          
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

          byte ii = 0;    
          char tStr[] = "AD99SUP";                                                               
          while ( DSKx.read() == tStr[ii++] ) {
          }   
          if ( ii < 6 ) {                                                                                 //not all characters matched so ...
            CALLstatus( DOADNotMapped );                                                                  //... no genuine ACFG DOAD mapped at DSK1
            noExec();                                                                                     //we kappen ermee
            break;  
          } 
     
          DSKx.seek( 0x21FF );                                                                            //ok genuine enough, get config data
          memset(&ACFG, 0, sizeof(AD99Config) );                                                          //clear configuration struct
          
          byte cc, octet;        
          for ( byte ii = 0; ii < 8; ii++ ) {                                                             //8 octets in total
            while ( DSKx.read() != 0x08 ) {                                                               //check for 0x08 delimiter
            }              
            cc = ( DSKx.read() & B00000011 );                                                             //check for 0x4x
            cc == 0 ? octet = DSKx.read() : octet = cc * (DSKx.read() * 100) + DSKx.read();               //x=0 so octet[0-16] || octet = x * (next byte * 100) + next byte            
            ii < 4 ? ACFG.ipaddress[ii] = octet : ACFG.ftpserver[ii-4] = octet;
          }
          
          while ( DSKx.read() == 0x00 ) {                                                                 //check for next delimiter (string length)
          }               
          DSKx.seek( DSKx.position() - 1 );                                                               //minus 1 to be able to re-read sring length (universal for() loop)
  
          char *ndevice;                                                                                  //pointer to relevant ACFG array
          for ( byte ii = 0; ii < 3; ++ii ) {                                                             //3 arrays in total:

            if ( ii == 0 ) {                                                                              //ntpserver = 0, ftpuser =1 and ftpass = 2
              ndevice = ACFG.ntpserver;
            } else {
              ii == 1 ? ndevice = ACFG.ftpuser : ndevice = ACFG.ftpass;
            }

            cc = DSKx.read();                                                                             //read string length ...
            byte jj;
            for ( jj = 0; jj < cc; ++jj ) {                                                               //... and save configuration string to relevant array 
              *(ndevice + jj) = DSKx.read();  
            }
            *(ndevice + jj) = '\0';                                                                       //terminate strings
          }

          DSKx.seek( DSKx.position() + 2 );                                                               //skip 0x4x (not used for 0-15 values)   
          ACFG.TZ = DSKx.read();                                                                          //store timezone value

          EEPROM.put( EEPROM_CFG,  ACFG );                                                                //save configuration struct to EEPROM

          noExec();                                                                                       //done
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
