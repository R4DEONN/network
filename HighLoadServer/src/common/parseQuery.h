#pragma once
#include <sstream>

#include "Query.h"

inline Query parseQuery(const std::string& input)
{
	std::istringstream iss(input);
	Query query{};

	if (!std::getline(iss, query.name))
	{
		throw std::invalid_argument("Invalid query string");
	}

	if (!(iss >> query.number))
	{
		throw std::invalid_argument("Invalid query number");
	}

	return query;
}