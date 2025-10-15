#pragma once
#include <memory>

#include "Socket.h"
#include "TcpClient.h"
#include <sys/socket.h>

class TcpServer : public Socket
{
public:
	TcpServer();
	bool bind(u_short port) const;
	bool listen(int backlog = SOMAXCONN) const;
	std::unique_ptr<TcpClient> accept() const;

	std::string getLocalAddress() const;
};
