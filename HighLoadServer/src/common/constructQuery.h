#pragma once
#include "Query.h"

inline std::string constructQuery(const Query& query)
{
	return query.name + "\n" + std::to_string(query.number) + "\n";
}
