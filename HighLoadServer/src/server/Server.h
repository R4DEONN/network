#pragma once

#include <string>
#include "../socket/EpollServer.h"

class Server
{
public:
	Server(unsigned short port, std::string name);
	void run();
	void shutdown();

private:
	EpollServer m_epollServer;
	std::string m_name;
	static constexpr int SERVER_NUMBER = 50;
};