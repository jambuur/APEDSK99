//-APEDSK99 specific-------------------------------------------------------------------------- FD1771 emu: variables and functions

//Status Register bits
#define NOTREADY  0x80
#define PROTECTED 0x40
#define NOERROR   0x00

//"Force Interrupt" command
#define FDINT 0xD0

//"disk" characteristics
#define NRTRACKS   40   //# of tracks/side
#define NRSECTS     9   //# sectors/track
#define NRBYSECT  256   //# bytes/sector

//CALL() buffer for DOAD and file name handling
#define CALLBF  0x1FD6
//APEDSK99-specific Command Register (TI BASIC CALL support)
#define ACOMND  0x1FE8
//R6 counter value to generate interrupt in read sector / track commands
#define RDINT   0x1FEA
//TI BASIC screen bias
#define TIBias  0x60
//pulling my hair out trouleshooting storage address
#define aDEBUG 0x1EE0

//CRU emulation bytes + FD1771 registers
#define CRURD   0x1FEC  //emulated 8 CRU input bits           (>5FEC in TI99/4a DSR memory block); not used but possible future use
//B00001111: DSK3 (0x06), DSK2 (0x04), DSK1 (0x02), side 0/1
#define CRUWRI  0x1FEE  //emulated 8 CRU output bits          (>5FEE in TI99/4a DSR memory block)
#define RSTAT   0x1FF0  //read FD1771 Status register         (>5FF0 in TI99/4a DSR memory block)
#define RTRACK  0x1FF2  //read FD1771 Track register          (>5FF2 in TI99/4a DSR memory block)
#define RSECTR  0x1FF4  //read FD1771 Sector register         (>55F4 in TI99/4a DSR memory block)
#define RDATA   0x1FF6  //read FD1771 Data register           (>5FF6 in TI99/4a DSR memory block)
#define WCOMND  0x1FF8  //write FD1771 Command register       (>5FF8 in TI99/4a DSR memory block)
#define WTRACK  0x1FFA  //write FD1771 Track register         (>5FFA in TI99/4a DSR memory block)
#define WSECTR  0x1FFC  //write FD1771 Sector register        (>5FFC in TI99/4a DSR memory block)
#define WDATA   0x1FFE  //write FD1771 Data register          (>5FFE in TI99/4a DSR memory block)

//DSKx file pointers
File DSK[4];  //file pointers to DOAD's

//flags for "drives" (aka DOAD files) available
boolean aDSK[4]   = {false, false, false, false};                                                 //disk availability
char nDSK[4][20]  = {"x", "/DISKS/_APEDSK1.DSK", "/DISKS/_APEDSK2.DSK", "/DISKS/_APEDSK3.DSK"};   //DOAD file names; startup defaults
byte pDSK[4]      = {0x00, 0x20, 0x20, 0x20};                                                     //DOAD write protect status
byte cDSK         = 0;                                                                            //current selected DSK

//various storage and flags for command interpretation and handling
volatile boolean FD1771   = false;  //interrupt routine flag: new or continued FD1771 command
byte FCcmd                = 0;      //current FD1771 command
byte FLcmd                = 0;      //last FD1771 command
boolean FNcmd             = false;  //flag new FD1771 command
byte ACcmd                = 0;      //current APEDSK99 command
byte ALcmd                = 0;      //last APEDSK99 command
boolean ANcmd             = false;  //flag new APEDSK99 command
unsigned long Dbtidx      = 0;      //absolute DOAD byte index
unsigned long Sbtidx      = 0;      // R/W sector/byte index counter
byte Ssecidx              = 0;      // R/W sector counter
boolean cDir              = HIGH;   //current step direction, step in(wards) towards track 39 by default
byte vDEBUG               = 0;      //generic pulling my hair out trouleshooting variable


//no further command execution (prevent seek/step commands to be executed multiple times)
void noExec(void) {
  DSK[cDSK].close();      //close current SD DOAD file
  Wbyte(WCOMND, FDINT);   //"force interrupt" command (aka no further execution)
  FCcmd = FDINT;          //"" ""
  FLcmd = FCcmd;          //reset new FD1771 command prep
  Wbyte(ACOMND, 0);       //clear APEDSK99 Command Register
  ACcmd = 0;              //reset APEDSK99 command
  ALcmd = ACcmd;          //reset new APEDSK99 command prep
  cDSK = 0;               //reset active DSKx
  Dbtidx = 0;             //clear absolute DOAD byte index
  Sbtidx = 0;             //clear byte index counter
  Ssecidx = 0;            //clear sector counter
}

//clear various FD1771 registers (for powerup and Restore command)
void FDrstr(void) {
  Wbyte(RSTAT,  0);       //clear Status Register
  Wbyte(RTRACK, 0);       //clear Read Track register
  Wbyte(RSECTR, 0);       //clear Read Sector register
  Wbyte(RDATA,  0);       //clear Read Data register
  Wbyte(WTRACK, 0);       //clear Write Track register
  Wbyte(WSECTR, 0);       //clear Write Sector register
  Wbyte(WDATA,  0);       //clear Write Data register
  Wbyte(RSTAT, NOERROR);  //clear Status register
}

//calculate and return absolute DOAD byte index for R/W commands
unsigned long cDbtidx (void) {
  unsigned long bi;
  bi  = ( Rbyte(CRUWRI) & B00000001 ) * NRTRACKS;     //add side 0 tracks (0x28) if on side 1
  bi += Rbyte(WTRACK);                                //add current track #
  bi *= NRSECTS;                                      //convert to # of sectors
  bi += Rbyte(WSECTR);                                //add current sector #
  bi *= NRBYSECT;                                     //convert to absolute DOAD byte index (max 184320 for DS/SD)
  return (bi);
}

void RWsector( boolean rw ) {
  if ( Sbtidx < NRBYSECT ) {                          //have we done all 256 bytes yet?
    if ( rw ) {                                       //no; do we need to read a sector?
      Wbyte(RDATA, DSK[cDSK].read() );                //yes -> supply next byte
    } else {
      DSK[cDSK].write( Rbyte(WDATA) );                //no -> next byte to DOAD
    }
    Sbtidx++;                                         //increase sector byte counter
  } else {
    if (FCcmd == 0x80 || FCcmd == 0xA0) {               //done with R/W single sector
      noExec();                                       //exit
    } else {
      if ( Ssecidx < (NRSECTS - 1) ) {                //last sector (8)?
        Ssecidx++;                                    //no; increase Sector #
        Wbyte(WSECTR, Ssecidx);                       //sync Sector Registers
        Wbyte(RSECTR, Ssecidx);                       //""
        if ( rw ) {                                   //do we need to read a sector?
          Wbyte(RDATA, DSK[cDSK].read() );            //yes -> supply next byte
        } else {
          DSK[cDSK].write( Rbyte(WDATA) );            //no -> next byte to DOAD
        } 
        Sbtidx = 1;                                   //adjust sector byte counter
      } else {
        noExec();                                     //all sectors done; exit
      }
    }
  }
}                                                     //yes; done all 256 bytes in the sector
