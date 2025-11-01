#include "Server.h"
#include <iostream>
#include <memory>
#include "../common/constructQuery.h"
#include "../common/parseQuery.h"
#include "../common/printInfo.h"

Server::Server(unsigned short port, std::string name)
	: m_epollServer(port)
	, m_name("Server of " + std::move(name))
{
}

void Server::run()
{
	m_epollServer.setMessageHandler([this](const std::string& request) -> std::string {
		try
		{
			const auto [clientName, clientNumber] = parseQuery(request);

			if (clientNumber < 0 || clientNumber > 100)
			{
				std::osyncstream(std::cout) << "Invalid client number (" << clientNumber << "), closing connection." << std::endl;
				return "";
			}

			printInfo(clientName, m_name, clientNumber, SERVER_NUMBER);

			return constructQuery({m_name, SERVER_NUMBER});
		}
		catch (const std::exception& e)
		{
			std::osyncstream(std::cerr) << "Error handling client " << ": " << e.what() << std::endl;
			return "";
		}
	});

	std::osyncstream(std::cout) << "Starting server on " << m_epollServer.getLocalAddress() << std::endl << std::endl;

	m_epollServer.run();
}

void Server::shutdown()
{
	m_epollServer.shutdown();
}
