#include "Socket.h"
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

Socket::Socket(int sock)
	: m_sock(sock)
{
}

Socket::~Socket()
{
	close();
}

Socket::Socket(Socket&& other) noexcept
	: m_sock(other.m_sock)
{
	other.m_sock = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept
{
	if (this != &other)
	{
		close();
		m_sock = other.m_sock;
		other.m_sock = -1;
	}
	return *this;
}

bool Socket::isValid() const
{
	return m_sock != -1;
}

int Socket::getHandle() const
{
	return m_sock;
}

void Socket::close()
{
	if (m_sock != -1)
	{
		::close(m_sock);
		std::cout << "Connection closed" << std::endl;
		m_sock = -1;
	}
}

int Socket::send(const char* data, int len) const
{
	if (!isValid())
	{
		throw std::runtime_error("Socket is not valid");
	}

	const int result = ::send(m_sock, data, len, 0);
	if (result == -1)
	{
		throw std::runtime_error("Socket send failed: " + std::string(strerror(errno)));
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
	if (result == -1)
	{
		throw std::runtime_error("Socket recv failed: " + std::string(strerror(errno)));
	}

	return result;
}