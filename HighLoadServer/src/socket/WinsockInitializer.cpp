#include "WinsockInitializer.h"

#include <stdexcept>
#include "windows.h"

WinsockInitializer::WinsockInitializer()
{
	WSADATA wsaData;
	int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (res != 0)
	{
		throw std::logic_error("WSAStartup failed: " + std::to_string(res));
	}
	initialized = true;
}

WinsockInitializer::~WinsockInitializer()
{
	if (initialized)
	{
		WSACleanup();
	}
}
