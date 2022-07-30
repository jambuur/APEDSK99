//NOTE: IPAddress uses a comma "," for the address definition instead of the familiair dot "." !

//CHANGE TO SUIT YOUR NETWORK
IPAddress       IP(192.168.1.2);                                                                  //EthernetShield IP address : 
IPAddress       ftpserver(192.168.1.3);                                                           //FTP server IP address
//IPAddress       namesServer(x,x,x,x);                                                           //not required if on same subnet as FTP server
//IPAddress       gateway(x,x,x,x);                                                               //not required if on same subnet as FTP server
//IPAddress       netmask(x,x,x,x);                                                               //not required if on same subnet as FTP server

//CHANGE TO SUIT YOUR NETWORK
const char *ntpserver = "192.168.1.3";                                                            //NTP servername or IP address
const char *user = "secret";                                                                      //FTP server username
const char *pass = "secret";                                                                      //FTP server password

//UNCOMMENT RELEVANT LINE TO SUIT YOUR TIMEZONE
//local hours difference with UTC in seconds (hours * 3600); 
//does not consider DST; adjust for DST by adding or substracting (hours * 3600) ie. 46300 - 3600

//#define TZ -43200         //International Date Line West, Etc/GMT+12
//#define TZ -39600         //Coordinated Universal Time-11, Etc/GMT+11
//#define TZ -36000         //Hawaii, Pacific/Honolulu
//#define TZ -32400         //Alaska, America/Anchorage
//#define TZ -28800         //Baja California, America/Santa_Isabel
//#define TZ -28800         //Pacific Time (US and Canada), America/Los_Angeles
//#define TZ -25200         //Chihuahua, La Paz, Mazatlan, America/Chihuahua
//#define TZ -25200         //Arizona, America/Phoenix
//#define TZ -25200         //Mountain Time (US and Canada), America/Denver
//#define TZ -21600         //Central America, America/Guatemala
//#define TZ -21600         //Central Time (US and Canada), America/Chicago
//#define TZ -21600         //Saskatchewan, America/Regina
//#define TZ -21600         //Guadalajara, Mexico City, Monterey, America/Mexico_City
//#define TZ -18000         //Bogota, Lima, Quito, America/Bogota
//#define TZ -18000         //Indiana (East), America/Indiana/Indianapolis
//#define TZ -18000         //Eastern Time (US and Canada), America/New_York
//#define TZ -14400         //Caracas, America/Caracas
//#define TZ -14400         //Atlantic Time (Canada), America/Halifax
//#define TZ -14400         //Asuncion, America/Asuncion
//#define TZ -14400         //Georgetown, La Paz, Manaus, San Juan, America/La_Paz
//#define TZ -14400         //Cuiaba, America/Cuiaba
//#define TZ -14400         //Santiago, America/Santiago
//#define TZ -10800         //Newfoundland, America/St_Johns
//#define TZ -10800         //Brasilia, America/Sao_Paulo
//#define TZ -10800         //Greenland, America/Godthab
//#define TZ -10800         //Cayenne, Fortaleza, America/Cayenne
//#define TZ -10800         //Buenos Aires, America/Argentina/Buenos_Aires
//#define TZ -10800         //Montevideo, America/Montevideo
//#define TZ -7200          //Coordinated Universal Time-2, Etc/GMT+2
//#define TZ -3600          //Cape Verde, Atlantic/Cape_Verde
//#define TZ -3600          //Azores, Atlantic/Azores
//#define TZ 0              //Casablanca, Africa/Casablanca
//#define TZ 0              //Monrovia, Reykjavik, Atlantic/Reykjavik
//#define TZ 0              //Dublin, Edinburgh, Lisbon, London, Europe/London
//#define TZ 0              //Coordinated Universal Time, Etc/GMT
//#define TZ 3600           //Amsterdam, Berlin, Bern, Rome, Stockholm, Vienna, Europe/Berlin
//#define TZ 3600           //Brussels, Copenhagen, Madrid, Paris, Europe/Paris
//#define TZ 3600           //West Central Africa, Africa/Lagos
//#define TZ 3600           //Belgrade, Bratislava, Budapest, Ljubljana, Prague, Europe/Budapest
//#define TZ 3600           //Sarajevo, Skopje, Warsaw, Zagreb, Europe/Warsaw
//#define TZ 3600           //Windhoek, Africa/Windhoek
//#define TZ 7200           //Athens, Bucharest, Istanbul, Europe/Istanbul
//#define TZ 7200           //Helsinki, Kiev, Riga, Sofia, Tallinn, Vilnius, Europe/Kiev
//#define TZ 7200           //Cairo, Africa/Cairo
//#define TZ 7200           //Damascus, Asia/Damascus
//#define TZ 7200           //Amman, Asia/Amman
//#define TZ 7200           //Harare, Pretoria, Africa/Johannesburg
//#define TZ 7200           //Jerusalem, Asia/Jerusalem
//#define TZ 7200           //Beirut, Asia/Beirut
//#define TZ 10800          //Baghdad, Asia/Baghdad
//#define TZ 10800          //Minsk, Europe/Minsk
//#define TZ 10800          //Kuwait, Riyadh, Asia/Riyadh
//#define TZ 10800          //Nairobi, Africa/Nairobi
//#define TZ 10800          //Tehran, Asia/Tehran
//#define TZ 14400          //Moscow, St. Petersburg, Volgograd, Europe/Moscow
//#define TZ 14400          //Tbilisi, Asia/Tbilisi
//#define TZ 14400          //Yerevan, Asia/Yerevan
//#define TZ 14400          //Abu Dhabi, Muscat, Asia/Dubai
//#define TZ 14400          //Baku, Asia/Baku
//#define TZ 14400          //Port Louis, Indian/Mauritius
//#define TZ 14400          //Kabul, Asia/Kabul
//#define TZ 18000          //Tashkent, Asia/Tashkent
//#define TZ 18000          //Islamabad, Karachi, Asia/Karachi
//#define TZ 18000          //Sri Jayewardenepura Kotte, Asia/Colombo
//#define TZ 18000          //Chennai, Kolkata, Mumbai, New Delhi, Asia/Kolkata
//#define TZ 18000          //Kathmandu, Asia/Kathmandu
//#define TZ 21600          //Astana, Asia/Almaty
//#define TZ 21600          //Dhaka, Asia/Dhaka
//#define TZ 21600          //Yekaterinburg, Asia/Yekaterinburg
//#define TZ 21600          //Yangon, Asia/Yangon
//#define TZ 25200          //Bangkok, Hanoi, Jakarta, Asia/Bangkok
//#define TZ 25200          //Novosibirsk, Asia/Novosibirsk
//#define TZ 28800          //Krasnoyarsk, Asia/Krasnoyarsk
//#define TZ 28800          //Ulaanbaatar, Asia/Ulaanbaatar
//#define TZ 28800          //Beijing, Chongqing, Hong Kong, Urumqi, Asia/Shanghai
//#define TZ 28800          //Perth, Australia/Perth
//#define TZ 28800          //Kuala Lumpur, Singapore, Asia/Singapore
//#define TZ 28800          //Taipei, Asia/Taipei
//#define TZ 32400          //Irkutsk, Asia/Irkutsk
//#define TZ 32400          //Seoul, Asia/Seoul
//#define TZ 32400          //Osaka, Sapporo, Tokyo, Asia/Tokyo
//#define TZ 32400          //Darwin, Australia/Darwin
//#define TZ 32400          //Adelaide, Australia/Adelaide
//#define TZ 36000          //Hobart, Australia/Hobart
//#define TZ 36000          //Yakutsk, Asia/Yakutsk
//#define TZ 36000          //Brisbane, Australia/Brisbane
//#define TZ 36000          //Guam, Port Moresby, Pacific/Port_Moresby
//#define TZ 36000          //Canberra, Melbourne, Sydney, Australia/Sydney
//#define TZ 39600          //Vladivostok, Asia/Vladivostok
//#define TZ 39600          //Solomon Islands, New Caledonia, Pacific/Guadalcanal
//#define TZ 43200          //Coordinated Universal Time+12, Etc/GMT-12
//#define TZ 43200          //Fiji, Marshall Islands, Pacific/Fiji
//#define TZ 43200          //Magadan, Asia/Magadan
#define TZ 43200          //Auckland, Wellington, Pacific/Auckland
//#define TZ 46800          //Nuku'alofa, Pacific/Tongatapu
//#define TZ 46800          //Samoa, Pacific/Apia

//ONLY CHANGE WHEN YOU EXPERIENCE PROBLEMS WITH DEFAULT SETTING
//short delay function to let bus/signals settle. 
//6us is the minumum stable value for HCT on my TI but your mileage may vary
inline void NOP() __attribute__((always_inline));
void NOP(){
  //try (3) for 74LS541 instead of the HCT versions but only when you experience problems. A good test is to format/verify a DS "floppy" a couple of times.
  //If that works, you're good. I only had to change this when initially using a couple of vague 74LS541's.
  delayMicroseconds(6);  
}
