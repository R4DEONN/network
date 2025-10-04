#include <iostream>
#include "Client.h"

Client::Client(std::string address, u_short port, std::string name)
	: m_address(std::move(address)),
	  m_port(port),
	  m_name(std::move(name))
{
}

void Client::run() const
{
	if (m_tcp.connect("127.0.0.1", m_port))
	{
		auto message = "Hello, my name is " + m_name + "!";
		m_tcp.sendString(message);
		auto response = m_tcp.receiveString();
		std::cout << "Response: " << response << std::endl;
	}
	// int number;
	// std::cin >> number;
	// std::cout << number << std::endl;
}
