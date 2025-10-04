#pragma once
#include "Socket.h"

class TcpClient : public Socket
{
public:
	TcpClient();
	TcpClient(SOCKET sock);
	bool connect(const std::string& ip, u_short port) const;

	int sendString(const std::string& str) const;
	std::string receiveString(size_t maxLen = 1024) const;
};
