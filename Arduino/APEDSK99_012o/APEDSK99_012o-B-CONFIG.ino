IPAddress       IP(10, 0, 0, 96);                                                                 //EthernetShield IP address : CHANGE TO SUIT YOUR NETWORK
IPAddress       ftpserver(10, 0, 0, 13);                                                          //FTP server IP address     : CHANGE TO SUIT YOUR NETWORK
//IPAddress       namesServer(8, 8, 8, 8);                                                        //not required if on same subnet as FTP server; otherwise CHANGE TO SUIT YOUR NETWORK
//IPAddress       gateway(10, 10, 10, 1);                                                         //not required if on same subnet as FTP server; otherwise CHANGE TO SUIT YOUR NETWORK
//IPAddress       netmask(255, 255, 255, 0);                                                      //not required if on same subnet as FTP server; otherwise CHANGE TO SUIT YOUR NETWORK

char *ntpserver = "10.0.0.13";                                                                    //NTP servername or IP address  : CHANGE TO SUIT YOUR NETWORK 
char *user = "apedsk99";                                                                          //FTP server username           : CHANGE TO SUIT YOUR NETWORK 
char *pass = "apedsk99";                                                                          //FTP server password           : CHANGE TO SUIT YOUR NETWORK 

#define TZ 46800                                                                                  //local hours difference with UTC in seconds (hours * 3600)

//short delay function to let bus/signals settle. 
//6us is the minumum stable value for HCT on my TI but your mileage may vary
//ONLY CHANGE WHEN YOU EXPERIENCE PROBLEMS WITH DEFAULT SETTING
inline void NOP() __attribute__((always_inline));
void NOP(){
  //try (3) for 74LS541 instead of the HCT versions but only when you experience problems. A good test is to format/verify a DS "floppy" a couple of times.
  //If that works, you're good. I only had to change this when initially using a couple of vague 74LS541's.
  delayMicroseconds(6);  
}
