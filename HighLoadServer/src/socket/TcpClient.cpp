#include "TcpClient.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

TcpClient::TcpClient()
	: Socket(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
{
	std::cout << "Client socket created" << std::endl;
	if (!isValid())
	{
		throw std::runtime_error("Invalid socket handle");
	}
}

TcpClient::TcpClient(int sock)
	: Socket(sock)
{
	if (!isValid())
	{
		throw std::runtime_error("Invalid socket handle");
	}
}

bool TcpClient::connect(const std::string& ip, u_short port) const
{
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip.c_str());

	const int result = ::connect(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	std::cout << "Client socket connected to " << ip << ":" << port << std::endl;

	return result != -1;
}

int TcpClient::sendString(const std::string& str) const
{
	std::cout << "Send: " << str << std::endl;
	return send(str.c_str(), static_cast<int>(str.length()));
}

std::string TcpClient::receiveString(size_t maxLen) const
{
	try
	{
		if (maxLen == 0)
		{
			return {};
		}

		std::vector<char> buffer(maxLen);
		const int bytes = recv(buffer.data(), static_cast<int>(maxLen));

		if (bytes < 0)
		{
			throw std::runtime_error("recv failed: " + std::string(strerror(errno)));
		}
		if (bytes == 0)
		{
			std::cout << "Connection closed by peer" << std::endl;
			return {};
		}

		std::string str(buffer.data(), static_cast<size_t>(bytes));
		std::cout << "Receive: " << str << std::endl;
		return str;
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}

	return "";
}

