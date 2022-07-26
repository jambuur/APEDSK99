/*
   Copyright (c) 2019 Boot&Work Corp., S.L. All rights reserved

   This library is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "NTP.h"

static const uint32_t seventyYears = 2208988800UL;

////////////////////////////////////////////////////////////////////////////////////////////////////
NTP::NTP(UDP &udp, uint16_t listenPort) : udp(udp), listenPort(listenPort), lastTime(0UL) {

}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool NTP::begin() {
	if (!udp.begin(listenPort)) {
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NTP::stop() {
	udp.stop();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool NTP::request(IPAddress ip, uint16_t port) {
	initRequest();

	return udp.beginPacket(ip, port) &&
		(udp.write(packet, NTP_PACKET_SIZE) >= NTP_PACKET_SIZE) &&
		udp.endPacket();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool NTP::request(const char *host, uint16_t port) {
	initRequest();

	return udp.beginPacket(host, port) &&
		(udp.write(packet, NTP_PACKET_SIZE) >= NTP_PACKET_SIZE) &&
		udp.endPacket();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int NTP::available() {
	if (udp.parsePacket() < NTP_PACKET_SIZE) {
		return 0;
	}

	udp.read(packet, NTP_PACKET_SIZE);

	uint32_t highWord = word(packet[40], packet[41]);
	uint32_t lowWord = word(packet[42], packet[43]);
	uint32_t secsSince1900 = highWord << 16 | lowWord;

	lastTime = secsSince1900 - seventyYears;

	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NTP::initRequest() {
	memset(packet, 0, NTP_PACKET_SIZE);

	packet[0] = 0b11100011; // LI, Version, Mode
	packet[1] = 0; // Stratum, or type of clock
	packet[2] = 6; // Polling Interval
	packet[3] = 0xEC; // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packet[12] = 49;
	packet[13] = 0x4E;
	packet[14] = 49;
	packet[15] = 52;
}
