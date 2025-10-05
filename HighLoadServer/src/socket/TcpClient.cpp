#include "TcpClient.h"

#include <iostream>
#include <stdexcept>
#include <vector>

TcpClient::TcpClient()
	: Socket(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
{
	std::cout << "Client socket created" << std::endl;
	if (!isValid())
	{
		throw std::runtime_error("Invalid socket handle");
	}
}

TcpClient::TcpClient(SOCKET sock)
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

	return result != SOCKET_ERROR;
}

int TcpClient::sendString(const std::string& str) const
{
	std::cout << "Send: " << str << std::endl;
	return send(str.c_str(), static_cast<int>(str.length()));
}

std::string TcpClient::receiveString(size_t maxLen) const
{
	std::vector<char> buffer(maxLen + 1);
	const int bytes = recv(buffer.data(), static_cast<int>(maxLen));
	const auto str = std::string(buffer.data(), bytes);
	std::cout << "Receive: " << str << std::endl;
	return str;
}

