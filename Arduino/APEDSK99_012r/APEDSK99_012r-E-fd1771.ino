//Status Register bits
#define NOTREADY  0x80
#define PROTECTED 0x40
#define NOERROR   0x00

//"Force Interrupt" command; the real thing interrupts execution and returns to a known state.
//Used in APEDSK99 as a "ready" flag
#define FDINT 0xD0

//"disk" characteristics
#define NRBYSECT  256                                                                                 //#bytes/sector
byte NRSECTS         =  9;                                                                            //#sectors/track (safe SD default)
unsigned int TNSECTS =  0;                                                                            //#sectors/DSK (max 1440)

#define ACOMND  0x5FE8                                                                                //APEDSK99-specific Command Register (TI BASIC CALL support)
#define RDINT   0x5FEA                                                                                //R6 counter value to generate interrupt in read sector command (see DSR source)

//CRU emulation bytes + FD1771 registers
#define CRUWRI  0x5FEE                                                                                //emulated 8 CRU output bits,     >5FEE in TI99/4a DSR memory block;
#define RSTAT   0x5FF0                                                                                //read FD1771 Status register,    >5FF0 in TI99/4a DSR memory block
#define RDATA   0x5FF6                                                                                //read FD1771 Data register,      >5FF6 in TI99/4a DSR memory block
#define WCOMND  0x5FF8                                                                                //write FD1771 Command register,  >5FF8 in TI99/4a DSR memory block
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
unsigned long DOADidx       = 0;                                                                      //absolute DOAD sector or byte index
unsigned int sectorbyteidx  = 0;                                                                      //R/W sector/byte index counter

//no further command execution 
void noExec( void ) {
  DSKx.close();                                                                                       //close current SD DOAD file
  write_DSRAM(WCOMND, FDINT);                                                                         //"force interrupt" command (aka no further execution)
  currentFD1771cmd            = FDINT;                                                                //"" ""
  lastFD1771cmd               = currentFD1771cmd;                                                     //reset new FD1771 command prep
  write_DSRAM(ACOMND, 0);                                                                             //clear APEDSK99 Command Register
  currentA99cmd               = 0;                                                                    //reset APEDSK99 command
  lastA99cmd                  = currentA99cmd;                                                        //reset new APEDSK99 command prep
  currentDSK                  = 0;                                                                    //reset active DSKx
  DOADidx                     = 0;                                                                    //clear absolute DOAD byte index
  sectorbyteidx               = 0;                                                                    //clear byte index counter
}

//clear various FD1771 registers (for powerup and Restore command)
void FD1771reset( void ) {
  write_DSRAM( RSTAT,  NOERROR );                                                                     //clear Status Register
  write_DSRAM( RDATA,  0 );                                                                           //clear Read Data register
  write_DSRAM( WSECTR, 0 );                                                                           //clear MSB Write Sector register
  write_DSRAM( WSECTR + 1, 0 );                                                                       //clear LSB Write Sector register
  write_DSRAM( WDATA,  0 );                                                                           //clear Write Data register
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
  }
}
