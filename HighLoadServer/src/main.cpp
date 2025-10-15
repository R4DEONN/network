#include <exception>
#include <iostream>
#include <optional>
#include <csignal>
#include "server/Server.h"
#include "client/Client.h"

struct Args
{
	bool isServer{};
	int port{};
	std::string address;
	std::string name;
};

std::optional<Args> ParseArgs(int argc, char** argv)
{
	Args args;

	if (argc == 3)
	{
		args.isServer = true;
		args.port = std::stoi(argv[1]);
		args.name = argv[2];
	}
	else if (argc == 4)
	{
		args.isServer = false;
		args.address = argv[1];
		args.port = std::stoi(argv[2]);
		args.name = argv[3];
	}
	else
	{
		return std::nullopt;
	}

	return args;
}

void waitForKillSignal()
{
	sigset_t set;
	int sig;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);

	pthread_sigmask(SIG_BLOCK, &set, nullptr);

	sigwait(&set, &sig);

	std::cout << "\nReceived signal " << sig << ". Shutting down..." << std::endl;
}

void RunImpl(const Args& args)
{
	if (args.isServer)
	{
		Server server(args.port, args.name);

		std::jthread serverThread([&server]() {
			try
			{
				server.run();
			}
			catch (const std::exception& e)
			{
				std::cerr << "Server error: " << e.what() << std::endl;
			}
		});

		waitForKillSignal();

		server.shutdown();
	}
	else
	{
		Client client(args.address, args.port, args.name);
		client.run();
	}
}

int main(int argc, char** argv)
{
	auto args = ParseArgs(argc, argv);
	if (!args)
	{
		std::cout
			<< "Usage" << std::endl
			<< "  Server mode: HighLoadServer <port> <name>\n" << std::endl
			<< "  Client mode: HighLoadServer <address> <port> <name>\n" << std::endl
			;
		return EXIT_FAILURE;
	}

	try
	{
		RunImpl(args.value());
	}
	catch (const std::exception& e)
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}