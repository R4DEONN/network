#pragma once

#include <string>

class Socket
{
public:
	explicit Socket(int sock = -1);
	virtual ~Socket();

	Socket(Socket&& other) noexcept;
	Socket& operator=(Socket&& other) noexcept;

	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;

	[[nodiscard]] bool isValid() const;
	[[nodiscard]] int getHandle() const;

	void close();

	int send(const char* data, int len) const;
	int recv(char* buffer, int len) const;

protected:
	int m_sock = -1;
};
