#pragma once

#include <string>

#include "winsock2.h"

class Socket
{
public:
	explicit Socket(SOCKET sock = INVALID_SOCKET);
	virtual ~Socket();

	Socket(Socket&& other) noexcept;
	Socket& operator=(Socket&& other) noexcept;

	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;

	[[nodiscard]] bool isValid() const;
	[[nodiscard]] SOCKET getHandle() const;

	void close();

	int send(const char* data, int len) const;
	int recv(char* buffer, int len) const;

protected:
	SOCKET m_sock = INVALID_SOCKET;
};
