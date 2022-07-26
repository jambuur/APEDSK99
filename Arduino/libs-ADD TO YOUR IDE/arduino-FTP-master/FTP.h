#ifndef __FTP_H__
#define __FTP_H__

#include <Arduino.h>
#include <Client.h>

#define _FTP_PORT 21
#define _FTP_TIMEOUT 15000UL
#define _FTP_CHUNK_SIZE	64

class FTP {
	public:
		explicit FTP(Client &cClient, Client &dClient);

	public:
		int connect(IPAddress ip, uint16_t port);
		int connect(const char *host, uint16_t port);
		int connect(IPAddress ip, uint16_t port, const char *user, const char *password = nullptr);
		int connect(const char *host, uint16_t port, const char *user, const char *password = nullptr);
		inline int connect(IPAddress ip) {
			return connect(ip, _FTP_PORT);
		}
		inline int connect(const char *host) {
			return connect(host, _FTP_PORT);
		}
		inline int connect(IPAddress ip, const char *user, const char *password = nullptr) {
			return connect(ip, _FTP_PORT, user, password);
		}
		inline int connect(const char *host, const char *user, const char *password = nullptr) {
			return connect(host, _FTP_PORT, user, password);
		}

		int auth(const char *user, const char *password);

		void stop();

		size_t retrieve(const char *fileName, Stream &stream);
		size_t store(const char *fileName, Stream &stream);

	private:
		int beginTransaction();
		int waitServerCode(const __FlashStringHelper *cmd, const char *arg, uint16_t response);
		uint16_t waitServerCode(const __FlashStringHelper *cmd, const char *arg = nullptr, char *desc = nullptr);
		uint16_t waitServerCode(char *desc = nullptr);
		void quit();

	private:
		Client &cClient;
		Client &dClient;
};

#endif // __FTP_H__
