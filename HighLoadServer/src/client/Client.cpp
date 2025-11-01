#include <iostream>
#include "Client.h"
#include "thread"

#include "../common/constructQuery.h"
#include "../common/parseQuery.h"
#include "../common/printInfo.h"

Client::Client(std::string address, u_short port, std::string name)
	: m_address(std::move(address)),
	  m_port(port),
	  m_name("Client of " + std::move(name))
{
}

void Client::run() const
{
	using namespace std::chrono_literals;
	int number;
	std::osyncstream(std::cout) << "Enter number: ";
	std::cin >> number;
	std::osyncstream(std::cout) << std::endl;

	if (!m_tcp.connect("127.0.0.1", m_port))
	{
		throw std::runtime_error("Failed to connect to 127.0.0.1:" + std::to_string(m_port));
	}

	auto message = constructQuery({m_name, number});
	m_tcp.sendString(message);
	auto response = m_tcp.receiveString();
	if (response.empty())
	{
		std::osyncstream(std::cout) << "Server closed connection" << std::endl;
		return;
	}
	auto [serverName, serverNumber] = parseQuery(response);
	printInfo(
		m_name,
		serverName,
		number,
		serverNumber
	);
}

void Client::run(int number, int sleepSec = -1) const
{
	if (sleepSec == -1)
	{
		run();
		return;
	}
	using namespace std::chrono_literals;
	if (!m_tcp.connect("127.0.0.1", m_port))
	{
		throw std::runtime_error("Failed to connect to 127.0.0.1:" + std::to_string(m_port));
	}

	std::this_thread::sleep_for(std::chrono::seconds(sleepSec));

	auto message = constructQuery({m_name, number});
	m_tcp.sendString(message);
	auto response = m_tcp.receiveString();
	if (response.empty())
	{
		std::osyncstream(std::cout) << "Server closed connection" << std::endl;
		return;
	}
	auto [serverName, serverNumber] = parseQuery(response);
	printInfo(
		m_name,
		serverName,
		number,
		serverNumber
	);
}
