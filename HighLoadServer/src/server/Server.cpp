#include "Server.h"

#include <iostream>

#include "../common/constructQuery.h"
#include "../common/parseQuery.h"
#include "../common/printInfo.h"
#include "../socket/TcpServer.h"

constexpr int SERVER_NUMBER = 50;

Server::Server(u_short port, std::string name)
	: m_name("Server of " + std::move(name)),
	  m_port(port)
{
}

void Server::run() const
{
	if (!m_tcp.bind(m_port) || !m_tcp.listen())
	{
		throw std::runtime_error("Server: failed to bind");
	}

	std::cout << "Starting server on " << m_tcp.getLocalAddress() << std::endl << std::endl;

	while (true)
	{
		auto client = m_tcp.accept();
		const std::string request = client->receiveString();
		const auto [clientName, clientNumber] = parseQuery(request);

		if (clientNumber < 0 || clientNumber > 100)
		{
			break;
		}

		printInfo(
			clientName,
			m_name,
			clientNumber,
			SERVER_NUMBER
		);

		const std::string message = constructQuery({m_name, SERVER_NUMBER});
		client->sendString(message);
	}
}
