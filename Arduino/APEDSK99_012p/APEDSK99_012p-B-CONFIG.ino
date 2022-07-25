//NOTE: IPAddress uses a comma "," for the address definition instead of the familiair dot "." !

//CHANGE TO SUIT YOUR NETWORK
IPAddress       IP(x,x,x,x);                                                                   //EthernetShield IP address : 
IPAddress       ftpserver(x,x,x,x);                                                              //FTP server IP address
//IPAddress       namesServer(x,x,x,x);                                                          //not required if on same subnet as FTP server
//IPAddress       gateway(x,x,x,x);                                                              //not required if on same subnet as FTP server
//IPAddress       netmask(x,x,x,x);                                                              //not required if on same subnet as FTP server

//CHANGE TO SUIT YOUR NETWORK
const char *ntpserver = "x,x,x,x";                                                              //NTP servername or IP address
const char *user = "xxxxxxxx";                                                                  //FTP server username
const char *pass = "xxxxxxxx";                                                                  //FTP server password

//UNCOMMENT RELEVANT LINE TO SUIT YOUR TIMEZONE
//local hours difference with UTC in seconds (hours * 3600); 
//does not consider DST; adjust for DST by adding or substracting (hours * 3600) ie. 46300 - 3600

//#DEFINE TZ -43200         //International Date Line West, Etc/GMT+12
//#DEFINE TZ -39600         //Coordinated Universal Time-11, Etc/GMT+11
//#DEFINE TZ -36000         //Hawaii, Pacific/Honolulu
//#DEFINE TZ -32400         //Alaska, America/Anchorage
//#DEFINE TZ -28800         //Baja California, America/Santa_Isabel
//#DEFINE TZ -28800         //Pacific Time (US and Canada), America/Los_Angeles
//#DEFINE TZ -25200         //Chihuahua, La Paz, Mazatlan, America/Chihuahua
//#DEFINE TZ -25200         //Arizona, America/Phoenix
//#DEFINE TZ -25200         //Mountain Time (US and Canada), America/Denver
//#DEFINE TZ -21600         //Central America, America/Guatemala
//#DEFINE TZ -21600         //Central Time (US and Canada), America/Chicago
//#DEFINE TZ -21600         //Saskatchewan, America/Regina
//#DEFINE TZ -21600         //Guadalajara, Mexico City, Monterey, America/Mexico_City
//#DEFINE TZ -18000         //Bogota, Lima, Quito, America/Bogota
//#DEFINE TZ -18000         //Indiana (East), America/Indiana/Indianapolis
//#DEFINE TZ -18000         //Eastern Time (US and Canada), America/New_York
//#DEFINE TZ -14400         //Caracas, America/Caracas
//#DEFINE TZ -14400         //Atlantic Time (Canada), America/Halifax
//#DEFINE TZ -14400         //Asuncion, America/Asuncion
//#DEFINE TZ -14400         //Georgetown, La Paz, Manaus, San Juan, America/La_Paz
//#DEFINE TZ -14400         //Cuiaba, America/Cuiaba
//#DEFINE TZ -14400         //Santiago, America/Santiago
//#DEFINE TZ -10800         //Newfoundland, America/St_Johns
//#DEFINE TZ -10800         //Brasilia, America/Sao_Paulo
//#DEFINE TZ -10800         //Greenland, America/Godthab
//#DEFINE TZ -10800         //Cayenne, Fortaleza, America/Cayenne
//#DEFINE TZ -10800         //Buenos Aires, America/Argentina/Buenos_Aires
//#DEFINE TZ -10800         //Montevideo, America/Montevideo
//#DEFINE TZ -7200          //Coordinated Universal Time-2, Etc/GMT+2
//#DEFINE TZ -3600          //Cape Verde, Atlantic/Cape_Verde
//#DEFINE TZ -3600          //Azores, Atlantic/Azores
//#DEFINE TZ 0              //Casablanca, Africa/Casablanca
//#DEFINE TZ 0              //Monrovia, Reykjavik, Atlantic/Reykjavik
//#DEFINE TZ 0              //Dublin, Edinburgh, Lisbon, London, Europe/London
//#DEFINE TZ 0              //Coordinated Universal Time, Etc/GMT
//#DEFINE TZ 3600           //Amsterdam, Berlin, Bern, Rome, Stockholm, Vienna, Europe/Berlin
//#DEFINE TZ 3600           //Brussels, Copenhagen, Madrid, Paris, Europe/Paris
//#DEFINE TZ 3600           //West Central Africa, Africa/Lagos
//#DEFINE TZ 3600           //Belgrade, Bratislava, Budapest, Ljubljana, Prague, Europe/Budapest
//#DEFINE TZ 3600           //Sarajevo, Skopje, Warsaw, Zagreb, Europe/Warsaw
//#DEFINE TZ 3600           //Windhoek, Africa/Windhoek
//#DEFINE TZ 7200           //Athens, Bucharest, Istanbul, Europe/Istanbul
//#DEFINE TZ 7200           //Helsinki, Kiev, Riga, Sofia, Tallinn, Vilnius, Europe/Kiev
//#DEFINE TZ 7200           //Cairo, Africa/Cairo
//#DEFINE TZ 7200           //Damascus, Asia/Damascus
//#DEFINE TZ 7200           //Amman, Asia/Amman
//#DEFINE TZ 7200           //Harare, Pretoria, Africa/Johannesburg
//#DEFINE TZ 7200           //Jerusalem, Asia/Jerusalem
//#DEFINE TZ 7200           //Beirut, Asia/Beirut
//#DEFINE TZ 10800          //Baghdad, Asia/Baghdad
//#DEFINE TZ 10800          //Minsk, Europe/Minsk
//#DEFINE TZ 10800          //Kuwait, Riyadh, Asia/Riyadh
//#DEFINE TZ 10800          //Nairobi, Africa/Nairobi
//#DEFINE TZ 10800          //Tehran, Asia/Tehran
//#DEFINE TZ 14400          //Moscow, St. Petersburg, Volgograd, Europe/Moscow
//#DEFINE TZ 14400          //Tbilisi, Asia/Tbilisi
//#DEFINE TZ 14400          //Yerevan, Asia/Yerevan
//#DEFINE TZ 14400          //Abu Dhabi, Muscat, Asia/Dubai
//#DEFINE TZ 14400          //Baku, Asia/Baku
//#DEFINE TZ 14400          //Port Louis, Indian/Mauritius
//#DEFINE TZ 14400          //Kabul, Asia/Kabul
//#DEFINE TZ 18000          //Tashkent, Asia/Tashkent
//#DEFINE TZ 18000          //Islamabad, Karachi, Asia/Karachi
//#DEFINE TZ 18000          //Sri Jayewardenepura Kotte, Asia/Colombo
//#DEFINE TZ 18000          //Chennai, Kolkata, Mumbai, New Delhi, Asia/Kolkata
//#DEFINE TZ 18000          //Kathmandu, Asia/Kathmandu
//#DEFINE TZ 21600          //Astana, Asia/Almaty
//#DEFINE TZ 21600          //Dhaka, Asia/Dhaka
//#DEFINE TZ 21600          //Yekaterinburg, Asia/Yekaterinburg
//#DEFINE TZ 21600          //Yangon, Asia/Yangon
//#DEFINE TZ 25200          //Bangkok, Hanoi, Jakarta, Asia/Bangkok
//#DEFINE TZ 25200          //Novosibirsk, Asia/Novosibirsk
//#DEFINE TZ 28800          //Krasnoyarsk, Asia/Krasnoyarsk
//#DEFINE TZ 28800          //Ulaanbaatar, Asia/Ulaanbaatar
//#DEFINE TZ 28800          //Beijing, Chongqing, Hong Kong, Urumqi, Asia/Shanghai
//#DEFINE TZ 28800          //Perth, Australia/Perth
//#DEFINE TZ 28800          //Kuala Lumpur, Singapore, Asia/Singapore
//#DEFINE TZ 28800          //Taipei, Asia/Taipei
//#DEFINE TZ 32400          //Irkutsk, Asia/Irkutsk
//#DEFINE TZ 32400          //Seoul, Asia/Seoul
//#DEFINE TZ 32400          //Osaka, Sapporo, Tokyo, Asia/Tokyo
//#DEFINE TZ 32400          //Darwin, Australia/Darwin
//#DEFINE TZ 32400          //Adelaide, Australia/Adelaide
//#DEFINE TZ 36000          //Hobart, Australia/Hobart
//#DEFINE TZ 36000          //Yakutsk, Asia/Yakutsk
//#DEFINE TZ 36000          //Brisbane, Australia/Brisbane
//#DEFINE TZ 36000          //Guam, Port Moresby, Pacific/Port_Moresby
//#DEFINE TZ 36000          //Canberra, Melbourne, Sydney, Australia/Sydney
//#DEFINE TZ 39600          //Vladivostok, Asia/Vladivostok
//#DEFINE TZ 39600          //Solomon Islands, New Caledonia, Pacific/Guadalcanal
//#DEFINE TZ 43200          //Coordinated Universal Time+12, Etc/GMT-12
//#DEFINE TZ 43200          //Fiji, Marshall Islands, Pacific/Fiji
//#DEFINE TZ 43200          //Magadan, Asia/Magadan
#DEFINE TZ 43200          //Auckland, Wellington, Pacific/Auckland
//#DEFINE TZ 46800          //Nuku'alofa, Pacific/Tongatapu
//#DEFINE TZ 46800          //Samoa, Pacific/Apia

//ONLY CHANGE WHEN YOU EXPERIENCE PROBLEMS WITH DEFAULT SETTING
//short delay function to let bus/signals settle. 
//6us is the minumum stable value for HCT on my TI but your mileage may vary
inline void NOP() __attribute__((always_inline));
void NOP(){
  //try (3) for 74LS541 instead of the HCT versions but only when you experience problems. A good test is to format/verify a DS "floppy" a couple of times.
  //If that works, you're good. I only had to change this when initially using a couple of vague 74LS541's.
  delayMicroseconds(6);  
}
