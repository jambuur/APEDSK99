//Status Register bits
#define NOTREADY  0x80
#define PROTECTED 0x40
#define NOERROR   0x00

//"Force Interrupt" command; the real thing interrupts execution and returns to a known state.
//Used in APEDSK99 as a "ready" flag
#define FDINT 0xD0

//"disk" characteristics
#define NRTRACKS   40                                                                                 //# tracks/side
#define NRSECTS     9                                                                                 //# sectors/track
#define NRBYSECT  256                                                                                 //# bytes/sector

#define ACOMND  0x5FE8                                                                                //APEDSK99-specific Command Register (TI BASIC CALL support)
#define RDINT   0x5FEA                                                                                //R6 counter value to generate interrupt in read sector / track commands (see DSR source)

//CRU emulation bytes + FD1771 registers
#define CRURD   0x5FEC                                                                                //emulated 8 CRU input bits,      >5FEC in TI99/4a DSR memory block; not currently used 
#define CRUWRI  0x5FEE                                                                                //emulated 8 CRU output bits,     >5FEE in TI99/4a DSR memory block;
#define RSTAT   0x5FF0                                                                                //read FD1771 Status register,    >5FF0 in TI99/4a DSR memory block
#define RTRACK  0x5FF2                                                                                //read FD1771 Track register,     >5FF2 in TI99/4a DSR memory block
#define RSECTR  0x5FF4                                                                                //read FD1771 Sector register,    >55F4 in TI99/4a DSR memory block
#define RDATA   0x5FF6                                                                                //read FD1771 Data register,      >5FF6 in TI99/4a DSR memory block
#define WCOMND  0x5FF8                                                                                //write FD1771 Command register,  >5FF8 in TI99/4a DSR memory block
#define WTRACK  0x5FFA                                                                                //write FD1771 Track register,    >5FFA in TI99/4a DSR memory block
#define WSECTR  0x5FFC                                                                                //write FD1771 Sector register,   >5FFC in TI99/4a DSR memory block
#define WDATA   0x5FFE                                                                                //write FD1771 Data register,     >5FFE in TI99/4a DSR memory block

#define READ    1                                                                                     //read sector flag
#define WRITE   0                                                                                     //write sector flag

byte currentFD1771cmd       = 0;                                                                      //current FD1771 command
byte lastFD1771cmd          = 0;                                                                      //last FD1771 command
boolean newFD1771cmd        = false;                                                                  //flag new FD1771 command
byte currentA99cmd          = 0;                                                                      //current APEDSK99 command
byte lastA99cmd             = 0;                                                                      //last APEDSK99 command
boolean newA99cmd           = false;                                                                  //flag new APEDSK99 command
unsigned long DOADbyteidx   = 0;                                                                      //absolute DOAD byte index
unsigned int sectorbyteidx  = 0;                                                                      //R/W sector/byte index counter
byte sectoridx              = 0;                                                                      //R/W sector counter
boolean currentStepDir      = HIGH;                                                                   //current step direction, step in(wards) towards highest track # by default

//no further command execution (prevent seek/step commands to be executed multiple times)
void noExec( void ) {
  DSKx.close();                                                                                       //close current SD DOAD file
  write_DSRAM(WCOMND, FDINT);                                                                         //"force interrupt" command (aka no further execution)
  currentFD1771cmd            = FDINT;                                                                //"" ""
  lastFD1771cmd               = currentFD1771cmd;                                                     //reset new FD1771 command prep
  write_DSRAM(ACOMND, 0);                                                                             //clear APEDSK99 Command Register
  currentA99cmd               = 0;                                                                    //reset APEDSK99 command
  lastA99cmd                  = currentA99cmd;                                                        //reset new APEDSK99 command prep
  currentDSK                  = 0;                                                                    //reset active DSKx
  DOADbyteidx                 = 0;                                                                    //clear absolute DOAD byte index
  sectorbyteidx               = 0;                                                                    //clear byte index counter
  sectoridx                   = 0;                                                                    //clear sector counter
}

//clear various FD1771 registers (for powerup and Restore command)
void FD1771reset( void ) {
  write_DSRAM( RSTAT,  0 );                                                                           //clear Status Register
  write_DSRAM( RTRACK, 0 );                                                                           //clear Read Track register
  write_DSRAM( RSECTR, 0 );                                                                           //clear Read Sector register
  write_DSRAM( RDATA,  0 );                                                                           //clear Read Data register
  write_DSRAM( WTRACK, 0 );                                                                           //clear Write Track register
  write_DSRAM( WSECTR, 0 );                                                                           //clear Write Sector register
  write_DSRAM( WDATA,  0 );                                                                           //clear Write Data register
  write_DSRAM( RSTAT, NOERROR );                                                                      //clear Status register
}

//calculate and return absolute DOAD byte index for R/W commands
unsigned long calcDOADidx ( void ) {
  unsigned long byteidx = ( read_DSRAM(CRUWRI) & B00000001 ) * NRTRACKS;                              //add side 0 tracks (0x28) if on side 1
                byteidx += read_DSRAM( WTRACK );                                                      //add current track #
                byteidx *= NRSECTS;                                                                   //convert to # of sectors
                byteidx += read_DSRAM( WSECTR );                                                      //add current sector #
                byteidx *= NRBYSECT;                                                                  //convert to absolute DOAD byte index (max 184320 for DS/SD)
  return ( byteidx );
}

void RWsector( boolean RW ) {
  switch ( currentFD1771cmd ) {

    case 0x80:                                                                                        //read sector
    case 0xA0:                                                                                        //write sector
    {
      if ( sectorbyteidx < NRBYSECT ) {                                                               //have we done all 256 bytes yet? ...
        if ( RW ) {                                                                                   //no; read sector?
          write_DSRAM( RDATA, DSKx.read() );                                                          //yes -> supply next byte
        } else {
          DSKx.write( read_DSRAM(WDATA) );                                                            //no -> write next byte to DOAD
        }
        sectorbyteidx++;                                                                              //increase sector byte counter
      } else {
        noExec();
      }
    }
    break;

    case 0x90: 
    case 0xE0:  
    case 0xB0:  
    case 0xF0: 
    { 
      if ( sectoridx < (NRSECTS - 1) ) {                                                              //last sector (8)?
        sectoridx++;                                                                                  //no; increase Sector #
        write_DSRAM( WSECTR, sectoridx );                                                             //sync Sector Registers
        write_DSRAM( RSECTR, sectoridx );                                                             //""
        if ( RW ) {                                                                                   //do we need to read a sector?
          write_DSRAM( RDATA, DSKx.read() );                                                          //yes -> supply next byte
        } else {
          DSKx.write( read_DSRAM(WDATA) );                                                            //no -> next byte to DOAD
        } 
        sectorbyteidx = 0;                                                                            //adjust sector byte counter
      } else {
        noExec();                                                                                     //all sectors done; exit
      }
    }
    break;
  }
}
