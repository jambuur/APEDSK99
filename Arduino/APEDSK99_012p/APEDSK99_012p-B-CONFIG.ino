//NOTE: IPAddress uses a comma "," for the address definition instead of the familiair dot "." !
IPAddress       IP(192,168,1,1);                                                                   //EthernetShield IP address : CHANGE TO SUIT YOUR NETWORK
IPAddress       ftpserver(192,168,1,100);                                                               //FTP server IP address     : CHANGE TO SUIT YOUR NETWORK
//IPAddress       namesServer(x,x,x,x);                                                           //not required if on same subnet as FTP server; otherwise CHANGE TO SUIT YOUR NETWORK
//IPAddress       gateway(x,x,x,x);                                                               //not required if on same subnet as FTP server; otherwise CHANGE TO SUIT YOUR NETWORK
//IPAddress       netmask(x,x,x,x);                                                               //not required if on same subnet as FTP server; otherwise CHANGE TO SUIT YOUR NETWORK

const char *ntpserver = "192.168.1.100";                                                                      //NTP servername or IP address  : CHANGE TO SUIT YOUR NETWORK 
const char *user = "xxxxxxxx";                                                                          //FTP server username           : CHANGE TO SUIT YOUR NETWORK 
const char *pass = "xxxxxxxx";                                                                          //FTP server password           : CHANGE TO SUIT YOUR NETWORK 

#define TZ 46800                                                                                  //local hours difference with UTC in seconds (hours * 3600); CHANGE TO SUIT YOUR TIMEZONE

//short delay function to let bus/signals settle. 
//6us is the minumum stable value for HCT on my TI but your mileage may vary
//ONLY CHANGE WHEN YOU EXPERIENCE PROBLEMS WITH DEFAULT SETTING
inline void NOP() __attribute__((always_inline));
void NOP(){
  //try (3) for 74LS541 instead of the HCT versions but only when you experience problems. A good test is to format/verify a DS "floppy" a couple of times.
  //If that works, you're good. I only had to change this when initially using a couple of vague 74LS541's.
  delayMicroseconds(6);  
}
