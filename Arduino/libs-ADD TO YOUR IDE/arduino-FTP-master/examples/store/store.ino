// FTP library example
// by Industrial Shields

#include <FTP.h>

#if defined(MDUINO_PLUS)
#include <Ethernet2.h>
#else
#include <Ethernet.h>
#endif

uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(10, 10, 10, 6);
IPAddress namesServer(8, 8, 8, 8);
IPAddress gateway(10, 10, 10, 1);
IPAddress netmask(255, 255, 255, 0);

IPAddress server(192, 168, 1, 220);
const char *user = "YOUR-USER";
const char *pass = "YOUR-PASSWORD";
const char *fileName = "YOUR-FILE";

EthernetClient ftpControl;
EthernetClient ftpData;

FTP ftp(ftpControl, ftpData);

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600UL);

  Ethernet.begin(mac, ip, namesServer, gateway, netmask);
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());

  if (!ftp.connect(server, user, pass)) {
    Serial.println("Error connecting to FTP server");
    while (true);
  }

  Serial.println("You have 10 seconds to write something...");
  delay(10000UL);

  // Send the written content to the FTP file
  ftp.store(fileName, Serial);

  Serial.println("The written content is sent to the FTP file");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  // Nothing to do
}
