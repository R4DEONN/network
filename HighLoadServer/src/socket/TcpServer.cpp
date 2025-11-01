#include "TcpServer.h"
#include "TcpClient.h"
#include <iostream>
#include <stdexcept>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <syncstream>

TcpServer::TcpServer()
	: Socket(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
{
	std::osyncstream(std::cout) << "Server socket created" << std::endl;
	if (!isValid())
	{
		throw std::runtime_error("Invalid socket: " + std::string(strerror(errno)));
	}

	int yes = 1;
	if (setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
	{
		std::cerr << "Warning: setsockopt(SO_REUSEADDR) failed" << std::endl;
	}
}

bool TcpServer::bind(unsigned short port) const
{
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	const int result = ::bind(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	if (result == 0)
	{
		std::osyncstream(std::cout) << "Server socket bound to port " << port << std::endl;
		return true;
	}
	else
	{
		std::osyncstream(std::cerr) << "Bind failed: " << strerror(errno) << std::endl;
		return false;
	}
}

bool TcpServer::listen(int backlog) const
{
	const int result = ::listen(m_sock, backlog);
	if (result == -1)
	{
		std::osyncstream(std::cerr) << "Listen failed: " << strerror(errno) << std::endl;
		return false;
	}
	return true;
}

std::unique_ptr<TcpClient> TcpServer::accept() const
{
	int client_fd = ::accept(m_sock, nullptr, nullptr);
	if (client_fd == -1)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			std::osyncstream(std::cerr) << "Accept failed: " << strerror(errno) << std::endl;
		}
		return nullptr;
	}

	return std::make_unique<TcpClient>(client_fd);
}

std::string TcpServer::getLocalAddress() const
{
	sockaddr_in addr{};
	socklen_t len = sizeof(addr);
	if (getsockname(m_sock, reinterpret_cast<sockaddr*>(&addr), &len) == 0)
	{
		char ipStr[INET_ADDRSTRLEN];
		if (inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr)))
		{
			return std::string(ipStr) + ":" + std::to_string(ntohs(addr.sin_port));
		}
	}
	return "unknown";
}