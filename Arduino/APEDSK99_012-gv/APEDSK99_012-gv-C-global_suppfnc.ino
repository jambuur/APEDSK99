//DSKx file pointers
File DSKx;                                                                                            //file pointer to current open DOAD
File SDdir;                                                                                           //file pointer for SD directory listing

//flags for "drives" (aka DOAD files) available 
boolean activeDSK[3]  = {false, false, false};                                                        //DSKx active flag
char DSKfolder[10]     = "DISKS/\0";                                                                   //root folder name
char nameDSK[3][22]   = {"DISKS/_APEDSK1.DSK", "DISKS/_APEDSK2.DSK", "DISKS/_APEDSK3.DSK"};           //DOAD file names; startup defaults
byte protectDSK[3]    = {0x20, 0x20, 0x20};                                                           //DOAD write protect status
byte currentDSK       = 0;                                                                            //current selected DSK
char DOADfullpath[22];                                                                                //complete path + filename
char DOADfilename[13];                                                                                //filename for DSK, DSR and /FOLDER
byte protectDOAD;                                                                                     //DSK image Protected flag

//command status and error handling
#define AVersion        0x4046                                                                        //DSR version string
#define DSKprm          0x5FB4                                                                        //per DSKx: Mbyte/Lbyte #sectors, #sectors/track, #tracks, #sides
#define CALLST          0x5FC6                                                                        //CALL() execution status                                                        
#define DOADNotMapped        0
#define DOADNotFound         1
#define FNTPConnect          2
#define Protected            3
#define DSRNotFound          4
#define DOADTooBig           5
#define DOADexists           6
#define DIRNotFound          7
#define FileNotFound         8                                                                        //not used currently
#define AInit                9
#define More              0xF0
#define AllGood           0xFF

#define CALLBF          0x5FC8                                                                        //CALL() buffer (data exchange)
#define TIBias            0x60                                                                        //TI BASIC screen bias
  
#define EEPROM_DSR         499                                                                        //EEPROM starting address for storing DSR filename (including \0)
#define EEPROM_CFG           0                                                                        //EEPROM starting address for APEDSK99 configuration

struct AD99Config {                                                                                   //struct for saving/loading APEDSK99 configuration
  byte ipaddress[4];                                                                                  //  current Ethernet Shield IP address
  byte ftpserver[4];                                                                                  //  current FTP server IP address
  char ntpserver[16];                                                                                 //  current NTP server IP address (including terminating \0)
  char ftpuser[9];                                                                                    //  current FTP server user name (including terminating \0)
  char ftpass[9];                                                                                     //  current FTP server password (including terminating \0)
  signed char TZ;                                                                                     //  current timezone value (hours difference with UTC)
} ACFG;                                                                                               //ACFG is stored in and loaded from EEPROM

//global counter (remember value in between successive calls of same APEDSK99 command)
byte gii = 0;

static const byte PROGMEM DaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};         //used in NTP month and day calculation
unsigned int TimeDateNum[5] = { 0, 0, 1, 1, 70 };                                                     //global array numeric NTP time/date

void EtherStart( void ) {
  pinAsOutput( ETH_CS );                                                                              //enable EthernetShield CS (shared with TI_INT)
  Ethernet.init( ETH_CS );                                                                            //activate Ethernet ...
  Ethernet.begin( MAC, ACFG.ipaddress );                                                              //... with set MAC address and IP address
}

void EtherStop( void ) {
  client.stop();                                                                                      //stop Ethernet client
  pinAsInput(MY_INT);                                                                                 //disable EthernetShield SS / enable TI_INT  
}

//per DSKx: Mbyte/Lbyte #sectors, #sectors/track, #tracks, #sides
boolean getDSKparms( byte cDSK ) {
  unsigned long cPos  = DSKx.position();                                                              //save current file position
  unsigned int  cPrms = DSKprm + (cDSK * 6);                                                          //point to right memory location for DSKx
  boolean  tooBig = false;                                                                            //we don't support >360KB DOAD's 

  DSKx.seek( 0x0A );                                                                                  //start with total #sectors
  byte Mbyte = DSKx.read();
  byte Lbyte = DSKx.read();
  if ( int( (Mbyte * 256) + Lbyte ) > 1440 ) {                                                        //more than 1440 AU's (sectors)?                                                
    tooBig = true;                                                                                    //yes -> "* DSK too large"
  } else {
    write_DSRAM( cPrms++, Mbyte );                                                                    //save #sectors Mbyte ...
    write_DSRAM( cPrms++, Lbyte );                                                                    //... and Lbyte
    write_DSRAM( cPrms++, DSKx.read() );                                                              //#sectors / track
    DSKx.seek( 0x11 );                                                                                //skip to ...
    write_DSRAM( cPrms++, DSKx.read() );                                                              //... #tracks ...
    write_DSRAM( cPrms  , DSKx.read() );                                                              //... and #sides
  }
  DSKx.seek( cPos );                                                                                  //return to initial file position
  return tooBig;
}

//read filename portion (without extension) into array
byte readCBuffer( void ) {
  memset( DOADfilename, 0x00, 13 );                                                                   //clear filename array  
  byte ii = 0;                                                                                        //start at DOADfilename[0]
  byte cc = 0xFF;                                                                                     //if cc becomes 0 loop ends
  while ( cc != 0x00 ) {                                                                              //max 8 characters for MSDOS filename                                                                                
    cc = read_DSRAM( CALLBF + (ii + 2) );                                                             //read character from APEDSK99 CALL buffer
    DOADfilename[ii++] = cc;                                                                          //add character
  }    
  return ii - 1;
}

boolean prepDSK( void ) {
  readCBuffer();                                                                                      //get filename  
  memset( DOADfullpath, 0x00, 22 );                                                                   //clear path array
  Acatpy( DOADfullpath, sizeof(DOADfullpath), DSKfolder, sizeof(DSKfolder) );                         //full file path starts with /FOLDER
  Acatpy( DOADfilename, sizeof(DOADfilename), ".DSK", 4);                                             //stick default file extension at the back       
  Acatpy( DOADfullpath, sizeof(DOADfullpath), DOADfilename, sizeof(DOADfilename) );                   //finalise full path
  
  boolean existDOAD = SD.exists( DOADfullpath );
  if ( existDOAD ) { 
    DSKx = SD.open( DOADfullpath, FILE_READ);                                                         //yes; open DOAD file
    DSKx.seek(0x10);                                                                                  //byte 0x10 in Volume Information Block stores status
    protectDOAD = DSKx.read();                                                                        //store Protected flag
  } 
  
  clrCALLbuffer();                                                                                    //clear CALL buffer for next one
  return existDOAD;                                                                                   //indicate DOAD exists or not
}

//fill CALL() buffer with " " for clean parameter-passing without left-over crap 
//i.e. shorter DOAD name following a longer one
void clrCALLbuffer( void ) {
  for ( byte ii = 0; ii < 32; ++ii ) {       
    write_DSRAM( CALLBF + ii, 0x20 + TIBias );                                                        //clear any leftover rubbish 
  }
}

//transfer relevant error message from error file to CALL buffer 
void CALLstatus( byte scode ) {
  write_DSRAM( CALLST, scode );                                                                       //update CALL status
  if ( scode < 99  ) {                                                                                //is it an actual error? (if not, no HONK; see DSR source)
    DSKx = SD.open( "/CALLerr.txt", FILE_READ );                                                      //open error text file
    clrCALLbuffer();                                                                                  //clear any leftover rubbish
    DSKx.seek( DSKx.position() + (scode * 16) );                                                      //seek proper error message position in file
    writeCBuffer( 2, 18, "\0", 0, true, false, '\0' );
  }
}

//generic function to write display data (and character definitions for ACHR) to the CALL buffer.
//usage: display column start, display column end+1, source array ("\0" if source = DSK), array index offset (0=no offset), 
//       source is DSK || array (true || false), break when cval is read (true || false), cval ('\0' when not used);
byte writeCBuffer( byte start, byte limit, char Asrc[], char SAoffset, boolean rDSK, boolean check, char cval ) {
  char cc;
  while ( start < limit ) {
    rDSK ? cc = DSKx.read() : cc = Asrc[start + SAoffset];                                            //read next DSK byte || next array value
    if ( check && cc == cval ) {                                                                      //have we come across the delimiter?
      break;                                                                                          //yes, we're done
    }
    write_DSRAM( CALLBF + (start), cc + TIBias );                                                     //nope, write character to CALL buffer
    start++;
  }
  return start;                                                                                       //handy for adding info in next column
}

//get UNIX timestamp from NTP server and store time/date in array
byte getNTPdt( void ) {
 
  unsigned long UNIXepoch, Hours, Minutes, Day, Month, Year;
 
  ntp.begin();                                                                                        //start NTP client
  if ( !ntp.request(ACFG.ntpserver) ) {                                                               //NTP server available?
    return ( FNTPConnect );                                                                           //no; report error
  } else {                                                                                            //yes; wait for NTP service availability
                                                                                       
    byte ii = 0;                                                                                      //check counter
    do {
      delay( 100 );                                                                                   //wait a bit
      ii++;
    } while ( !ntp.available() && ii < 25 );                                                          //wait a bit more if NTP is not available yet
    
    UNIXepoch = ntp.read() + ( long(ACFG.TZ) * long(3600) );                                          //get NTP UNIX timestamp and adjust for local time zone
      
    Minutes  = UNIXepoch  / 60;                                                                       //calculate minutes
    Hours    = Minutes    / 60;                                                                       //calculate hours
    Minutes -= Hours      * 60;
    Day      = Hours      / 24;                                                                       //calculate days                                                                 
    Hours   -= Day        * 24;
  
    Year      = 1970;                                                                                 //UNIX time starts in 1970
  
    while( true ) {
      bool          LeapYear   = (Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0));             //calculate year
      unsigned int  DaysInYear = LeapYear ? 366 : 365;
      if ( Day >=  DaysInYear) {
        Day       -= DaysInYear;
        Year++;
      } else {
        
        for( Month = 0; Month < 12; ++Month ) {                                                       //calculate month
          byte ii = pgm_read_byte( &DaysInMonth[Month] );                                             //read #days/month table in PROGMEM
          if ( Month == 1 && LeapYear ) {                                                             //add a day to Feburary in a leap year
            ii++;
          }
          if ( Day >= ii ) {                                                                          //calculate day in month
            Day -= ii;
          } else {
            break;
          }
        }
        break;
      }
    }
    TimeDateNum[0] = (unsigned int)Hours;                                                             //store date and time in global array
    TimeDateNum[1] = (unsigned int)Minutes;
    TimeDateNum[2] = (unsigned int)++Day;
    TimeDateNum[3] = (unsigned int)++Month;
    TimeDateNum[4] = (unsigned int)Year;

    return ( AllGood );                                                                               //we've got the time baby
  } 
  ntp.stop();                                                                                         //stop ntp client
}

//write NTP date/time data from array to FAT
void writeFATts( void ) {                                                                             //update FAT MODIFY time/date
  if ( getNTPdt() != FNTPConnect ) {                                                                  //if valid NTP time ...
    DSKx.timestamp( T_WRITE, TimeDateNum[4],                                                          //... update FAT data
                             TimeDateNum[3],
                             TimeDateNum[2],
                             TimeDateNum[0],
                             TimeDateNum[1],
                                        0 ); 
  }
}

//read FAT time/date data into array
byte readFATts( void ) {                                                                              //read FAT MODIFY time/date
  dir_t DIR;
  if ( DSKx.dirEntry(&DIR) ) {                                                                        //only read FAT time/date if valid directory entry                 
    TimeDateNum[4] = (int)FAT_YEAR  (DIR.lastWriteDate);                                              //store FAT date and time in global array   
    TimeDateNum[3] = (int)FAT_MONTH (DIR.lastWriteDate);
    TimeDateNum[2] = (int)FAT_DAY   (DIR.lastWriteDate);
    TimeDateNum[0] = (int)FAT_HOUR  (DIR.lastWriteTime);
    TimeDateNum[1] = (int)FAT_MINUTE(DIR.lastWriteTime);
  } 
}

//convert NTP array to a nice string format and write to CALL buffer
void converTD( byte CBstartcol, boolean sYear ) {
  rJust( CBstartcol,  CBstartcol + 2, TimeDateNum[0] );                                               //write right-justified hours to CALL buffer
  write_DSRAM( CALLBF +  (CBstartcol +  2), ':' + TIBias );                                           //write delimiter to CALL buffer
  rJust( CBstartcol +  3, CBstartcol +  5, TimeDateNum[1] );                                          //minutes 

  rJust( CBstartcol +  6, CBstartcol +  8, TimeDateNum[2] );                                          //day
  write_DSRAM( CALLBF + ( CBstartcol +  8), '/' + TIBias ); 
  if ( sYear ) {                                                                                      //short year?
    rJust( CBstartcol + 10, CBstartcol + 14, TimeDateNum[4] );                                        //yes, century overwritten by month
  } else {
    rJust( CBstartcol + 12, CBstartcol + 16, TimeDateNum[4] );                                        //nope, full year (TIME)
  }
  rJust( CBstartcol +  9, CBstartcol + 11, TimeDateNum[3] );                                          //month
  write_DSRAM( CALLBF + ( CBstartcol + 11), '/' + TIBias );

  for ( byte ii = CBstartcol; ii < CBstartcol + 12; ii += 3 ) {                                       //fill in the blanks; add leading zero to 1 digit hour, minute, day and month
    if ( read_DSRAM( CALLBF + ii ) == ' ' + TIBias ) {
      write_DSRAM( CALLBF + ii, '0' + TIBias );
    }
  }
}

//homegrown (read simple) sprintf; ASCII version of value is displayed right-justified 
//usage: display column start, display column end+1, value to be displayed
void rJust( byte CBstartcol, byte CBendcol, long lval ) {                                          
  char valChar[11];
  memset( valChar, 0x20, 10 );                                                                        //clear array ...
  ltoa( lval, valChar, 10 );                                                                          //... and store long value in ASCII
  byte valIndex = 0;
  for ( byte ii = CBstartcol; ii < CBendcol; ++ii ) {                                                 //print ASCII value to CALL buffer ...
    write_DSRAM( CALLBF + ii, (valChar[CBendcol-ii] != 0x20 ? valChar[valIndex++] : ' ') + TIBias );  //... checking from end to start for space (save ' ' -> next) or !space (ASCII char -> next)
  }
}

//homegrown (read simple) mix of strncpy and strncat. 
//WARNING: only works with char/byte arrays and NOT MUCH ERROR CHECKING is done.
//expects destination array insertion point to be 0x00 
//usage: destination array, destination array size, source array||string, # of characters to transfer   
byte Acatpy( char Adest[], byte dSize, char Asrc[], byte sSize ) {
  byte ii;                                                                                            
  dSize--;                                                                                            //adjust max destination array index (need to add at least 1 character)
  for ( ii = 0; ii < dSize; ++ii ) {                                                                  //find 0x00 delimiter; will be [0] if cleared array
    if ( Adest[ii] == 0x00 ) {
      break;
    } 
  }
  
  for ( byte jj = 0; jj < sSize; ++jj ) {                                                             //add source array to destination array ...
    if ( Asrc[jj] != 0x00 ) {                                                                         //... but make sure we don't go past max src array ...
      if ( ii < dSize ) {                                                                             //... and dest array sizes
        Adest[ii++] = Asrc[jj];
      }
    } else {
      break;
    }
  }
  return ii;                                                                                          //could be useful, not used currently
}

//homegrown (read simple) version of strncmp
//usage: array to check, size of array to check, array to be checked against
boolean Acomp( char Achk[], byte dSize, char Asrc[] ) {
  boolean same =  true;
  for ( byte ii = 0; ii < dSize; ++ii ) {                                                             //check array elements one by one
    if ( Achk[ii] != Asrc[ii] ) {                                                                     //any difference -> set flag
      same = false; 
    }
  }
  return same;
}
