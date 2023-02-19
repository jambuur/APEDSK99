//DSKx file pointers
File DSKx;                                                                                            //file pointer to current open DOAD
File SDdir;                                                                                           //file pointer for SD directory listing

//flags for "drives" (aka DOAD files) available 
boolean activeDSK[3]  = {false, false, false};                                                        //DSKx active flag
char DSKfolder[8]     = "/DISKS/\0";                                                                  //root folder name
char nameDSK[3][20]   = {"/DISKS/_APEDSK1.DSK", "/DISKS/_APEDSK2.DSK", "/DISKS/_APEDSK3.DSK"};        //DOAD file names; startup defaults
byte protectDSK[3]    = {0x20, 0x20, 0x20};                                                           //DOAD write protect status
byte currentDSK       = 0;                                                                            //current selected DSK

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
#define DEFNotFound          8
#define AInit                9
#define More              0xF0
#define AllGood           0xFF

#define CALLBF          0x5FC8                                                                        //CALL() buffer (data exchange)
#define TIBias          0x60                                                                          //TI BASIC screen bias

#define EEPROM_DSR      499                                                                           //EEPROM starting address for storing DSR filename (including \0)
#define EEPROM_CFG        0                                                                           //EEPROM starting address for APEDSK99 configuration

struct AD99Config {                                                                                   //struct for saving/loading APEDSK99 configuration
  byte ipaddress[4];                                                                                  //current Ethernet Shield IP address
  byte ftpserver[4];                                                                                  //current FTP server IP address
  char ntpserver[16];                                                                                 //current NTP server IP address (including terminating \0)
  char ftpuser[9];                                                                                    //current FTP server user name (including terminating \0)
  char ftpass[9];                                                                                     //current FTP server password (including terminating \0)
  signed char TZ;                                                                                     //current timezone value (hours difference with UTC)
} ACFG;                                                                                               //ACFG is stored in and loaded from EEPROM

//global counter (remember value in between successive calls of same APEDSK99 command)
byte gii = 0;

static const byte PROGMEM DaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};         //used in NTP month and day calculation
unsigned int TimeDateNum[5] = { 0, 0, 1, 1, 70 };                                                     //global array numeric NTP time/date
char TimeDateASC[17] = "\0";                                                                          //global array ASCII NTP time/date

//(error) messages in FLASH memory
const char CALLerror[10][16] PROGMEM = {                                                              //CALL() (error) messages
  { "* DSK not mapped" },
  { "* DSK not found " }, 
  { "* no FTP or NTP " },
  { "* DSK protected " },
  { "* DSR not found " },
  { "* DSK size error" },
  { "* DSK exists    " },
  { "* /DIR not found" },
  { "* no char file  " },
  { "APEDSK99 OK     " },
};

//help message in FLASH memory
const char CALLhelp[18][29] PROGMEM = {                                                                 //CALL() help text
  { "ARST = soft reset APEDSK99   " },
  { "LDIR = list doads on sd card " },
  { "SMAP = show DSK[1-3] mapping " },
  { "TIME = ntp date/time (NTP$)  " },
  { "ACHR = load real lower case  " },
  { "-                            " },
  { "LDSK(#) = list files on DSK# " },
  { "PDSK(#) = protect DSK#       " },     
  { "UDSK(#) = unprotect DSK#     " },
  { "-                            " },
  { "MDSK(#,\"1-8$\")=map DSK#->doad" },
  { "NDSK(#,\"1-8$\")=rename DSK#   " },
  { "-                            " },
  { "CDIR(\"1-5$\")=change /DIR   " },
  { "RDSK(\"1-8$\")=delete sd doad  " },
  { "FGET(\"1-8$\")=doad ftp -> sd  " },
  { "FPUT(\"1-8$\")=doad sd  -> ftp " },
  { "ADSR(\" =8$\")=load DSR & reset" }, 
};

//reset Arduino properly via watchdog timer
void APEDSK99reset ( void ) {
  wdt_disable();                                                                                      //disable Watchdog (just to be sure)                              
  wdt_enable(WDTO_15MS);                                                                              //enable it and set delay to 15ms
  while( true ) {}                                                                                    //self-destruct in 15ms
}

//unrecoverable errors: flash error code until reset (TI still usable)
//  flash             : SPI, SD Card fault / not ready
//  flash-flash       : no valid DSR binary image
//  flash-flash-flash : no valid DSR header (0xAA) at DSR RAM 0x0000)
void Flasher( byte errorcode ) {                                                                      //error routine: stuck in code flashing loop until reset
  //"no APEDSK99 for you"
  TIstop();                                                                                           //stop TI ...
  digitalHigh(TI_READY);                                                                              //... but let user enjoy vanilla console
  
  while ( true ) { 
    for ( byte flash = 0; flash < errorcode; flash++ ) {
      digitalLow(CE);                                                                                 //set RAM CE* LOW, turns on LED
      delay(LED_ON);                                                                                  //LED is on for a bit
      
      digitalHigh(CE);                                                                                //turn it off
      delay(LED_OFF);                                                                                 //LED is off for a bit
    }
    delay(LED_REPEAT);                                                                                //allow human interpretation
  }
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

//transfer relevant error message from FLASH memory to CALL buffer 
void CALLstatus( byte scode ) {
  write_DSRAM( CALLST, scode );                                                                       //update CALL status
  if ( scode < 99  ) {                                                                                //is it an actual error? (if not, no HONK; see DSR source)
    clrCALLbuffer();
    for ( byte ii = 2; ii < 18; ii++ ) {                                                              //yes; copy error message to CALL buffer
      write_DSRAM( CALLBF + ii, pgm_read_byte( &CALLerror[scode][ii-2] ) + TIBias );                  //2 leading spaces to mimic TI BASIC error messages
    }
  }
}

//fill CALL() buffer with " " for clean parameter-passing without left-over crap 
//i.e. shorter DOAD name following a longer one
void clrCALLbuffer( void ) {
  for ( byte ii = 0; ii < 32; ii++ ) {       
    write_DSRAM( CALLBF + ii, 0x20 + TIBias );                                                        //clear CALL buffer
  }
}

void EtherStart( void ) {
  pinAsOutput( ETH_CS );                                                                              //enable EthernetShield CS (shared with TI_INT)
  Ethernet.init( ETH_CS );                                                                            //activate Ethernet ...
  Ethernet.begin( MAC, ACFG.ipaddress );                                                              //... with set MAC address and IP address
}

void EtherStop( void ) {
  client.stop();                                                                                      //stop Ethernet client
  pinAsInput(TI_INT);                                                                                 //disable EthernetShield SS / enable TI_INT  
}

//get UNIX timestamp from NTP server and return "HH:mm DD/MM/YYYY" to CALL buffer
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

void writeFATts( void ) {                                                                             //update FAT MODIFY time/date
  byte chkNTPdt = getNTPdt();
  if ( chkNTPdt != FNTPConnect ) {                                                                    //if valid NTP time ...
    DSKx.timestamp(T_WRITE, TimeDateNum[4],                                                           //... update FAT data
                                       TimeDateNum[3],
                                       TimeDateNum[2],
                                       TimeDateNum[0],
                                       TimeDateNum[1],
                                                    0 ); 
  }
}

byte readFATts( void ) {                                                                              //read FAT MODIFY time/date
  dir_t DIR;
  TimeDateASC[0] = '\0';
  
  if ( DSKx.dirEntry(&DIR) ) {                                                                        //only read FAT time/date if valid directory entry                 
    TimeDateNum[4] = (int)FAT_YEAR  (DIR.lastWriteDate);                                              //store FAT date and time in global array   
    TimeDateNum[3] = (int)FAT_MONTH (DIR.lastWriteDate);
    TimeDateNum[2] = (int)FAT_DAY   (DIR.lastWriteDate);
    TimeDateNum[0] = (int)FAT_HOUR  (DIR.lastWriteTime);
    TimeDateNum[1] = (int)FAT_MINUTE(DIR.lastWriteTime);
  } 
}

void converTD( void ) {
  sprintf( TimeDateASC, "%02u:%02u %02u/%02u/%04u", TimeDateNum[0],                                   //nicely formatted time/date string in global array for SDSK() and TIME()
                                                    TimeDateNum[1],
                                                    TimeDateNum[2],
                                                    TimeDateNum[3],
                                                    TimeDateNum[4] );  
}
