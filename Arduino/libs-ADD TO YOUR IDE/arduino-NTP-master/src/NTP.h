#ifndef __NTP_H__
#define __NTP_H__

#include <Arduino.h>
#include <Udp.h>

#define NTP_PORT 123
#define NTP_PACKET_SIZE 48

class NTP {
	public:
		explicit NTP(UDP &udp, uint16_t listenPort = NTP_PORT);

	public:
		bool begin();
		void stop();

		int available();
		bool request(IPAddress ip, uint16_t port = NTP_PORT);
		bool request(const char *host, uint16_t port = NTP_PORT);

		inline uint32_t read() const {
			return lastTime;
		}

	private:
		void initRequest();

	private:
		UDP &udp;
		uint16_t listenPort;
		uint32_t lastTime;
		uint8_t packet[NTP_PACKET_SIZE];
};

#endif
