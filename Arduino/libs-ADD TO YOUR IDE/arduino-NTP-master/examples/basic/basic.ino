// NTP library example
// by Industrial Shields

#if defined(MDUINO_PLUS)
#include <Ethernet2.h>
#else
#include <Ethernet.h>
#endif

#include <NTP.h>

uint8_t mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 10, 10, 9);
IPAddress namesServer(8, 8, 8, 8);
IPAddress gateway(10, 10, 10, 1);
IPAddress netmask(255, 255, 255, 0);

EthernetUDP udp;
NTP ntp(udp);

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600UL);

  // Start Ethernet and NTP
  Ethernet.begin(mac, ip, namesServer, gateway, netmask);
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());

  ntp.begin();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  // Send periodic NTP requests
  static uint32_t lastRequestTime = 0UL;
  if (millis() - lastRequestTime > 10000UL) {
    if (!ntp.request("0.es.pool.ntp.org")) {
      Serial.println("Request error");
    }

    lastRequestTime = millis();
  }

  // Process NTP responses
  if (ntp.available()) {
    uint32_t timestamp = ntp.read();
    Serial.print("NTP timestamp: ");
    Serial.println(timestamp);
  }
}
