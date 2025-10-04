#pragma once

class WinsockInitializer
{
public:
	WinsockInitializer();

	~WinsockInitializer();

	WinsockInitializer(const WinsockInitializer&) = delete;

	WinsockInitializer &operator=(const WinsockInitializer&) = delete;

private:
	bool initialized = false;
};
