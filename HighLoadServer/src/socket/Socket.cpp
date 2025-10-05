#include "Socket.h"

#include <iostream>
#include <stdexcept>

Socket::Socket(SOCKET sock)
{
	m_sock = sock;
}

Socket::~Socket()
{
	close();
}

Socket::Socket(Socket&& other) noexcept
{
	m_sock = other.m_sock;
}

Socket &Socket::operator=(Socket&& other) noexcept
{
	if (this != &other)
	{
		close();
		m_sock = other.m_sock;
		other.m_sock = INVALID_SOCKET;
	}

	return *this;
}

bool Socket::isValid() const
{
	return m_sock != INVALID_SOCKET;
}

SOCKET Socket::getHandle() const
{
	return m_sock;
}

void Socket::close()
{
	if (m_sock != INVALID_SOCKET)
	{
		closesocket(m_sock);
		std::cout << "Connection closed" << std::endl;
		m_sock = INVALID_SOCKET;
	}
}

int Socket::send(const char* data, int len) const
{
	if (!isValid())
	{
		throw std::runtime_error("Socket is not valid");
	}

	const int result = ::send(m_sock, data, len, 0);
	if (result == SOCKET_ERROR)
	{
		throw std::runtime_error("Socket send failed " + std::to_string(WSAGetLastError()));
	}

	return result;
}

int Socket::recv(char* buffer, int len) const
{
	if (!isValid())
	{
		throw std::runtime_error("Socket is not valid");
	}
	const int result = ::recv(m_sock, buffer, len, 0);
	if (result == SOCKET_ERROR)
	{
		throw std::runtime_error("Socket recv failed " + std::to_string(WSAGetLastError()));
	}

	return result;
}
