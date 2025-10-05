#include "TcpServer.h"
#include <winsock.h>
#include <ws2tcpip.h>

TcpServer::TcpServer()
	: Socket(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
{
	if (!isValid())
	{
		throw std::runtime_error("Invalid socket");
	}
}

bool TcpServer::bind(u_short port) const
{
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	const int result = ::bind(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

	return result != SOCKET_ERROR;
}

bool TcpServer::listen(int backlog) const
{
	return ::listen(m_sock, backlog) != SOCKET_ERROR;
}

std::unique_ptr<TcpClient> TcpServer::accept() const
{
	SOCKET client = ::accept(m_sock, nullptr, nullptr);
	if (client == INVALID_SOCKET)
	{
		return nullptr;
	}

	return std::make_unique<TcpClient>(client);
}

std::string TcpServer::getLocalAddress() const
{
	sockaddr_in addr;
	int len = sizeof(addr);
	if (getsockname(m_sock, reinterpret_cast<sockaddr*>(&addr), &len) == 0)
	{
		char ipStr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr.sin_addr.s_addr, ipStr, sizeof(ipStr));
		return std::string(ipStr) + ":" + std::to_string(ntohs(addr.sin_port));
	}

	return "unknown";
}
