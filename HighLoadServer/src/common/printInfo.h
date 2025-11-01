#pragma once
#include <iostream>
#include <string>
#include <syncstream>

inline void printInfo(
	const std::string& clientName,
	const std::string& serverName,
	int clientNumber,
	int serverNumber
)
{
	std::osyncstream(std::cout)
		<< "Client: " << clientName << std::endl
		<< "Server: " << serverName << std::endl
		<< "Client number: " << clientNumber << std::endl
		<< "Server number: " << serverNumber << std::endl
		<< "Sum: " << clientNumber + serverNumber << std::endl
		<< std::endl;
}
