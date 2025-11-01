#include <exception>
#include <iostream>
#include <optional>
#include <csignal>
#include <thread>
#include <random>
#include <vector>
#include <chrono>
#include "server/Server.h"
#include "client/Client.h"

struct Args
{
	enum class Mode
	{
		Server,
		SingleClient,
		LoadClient
	};

	Mode mode = Mode::Server;
	int port{};
	std::string address;
	std::string name;
	int instanceCount = 1;
};

std::optional<Args> ParseArgs(int argc, char** argv)
{
	Args args;

	if (argc == 3)
	{
		// Server mode: ./app <port> <name>
		args.mode = Args::Mode::Server;
		args.port = std::stoi(argv[1]);
		args.name = argv[2];
	}
	else if (argc == 4)
	{
		// Single client: ./app <addr> <port> <name>
		args.mode = Args::Mode::SingleClient;
		args.address = argv[1];
		args.port = std::stoi(argv[2]);
		args.name = argv[3];
	}
	else if (argc == 5)
	{
		// Load test: ./app <addr> <port> <base_name> <count>
		args.mode = Args::Mode::LoadClient;
		args.address = argv[1];
		args.port = std::stoi(argv[2]);
		args.name = argv[3];
		args.instanceCount = std::stoi(argv[4]);
		if (args.instanceCount <= 0)
			return std::nullopt;
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

	sigwait(&set, &sig);

	std::cout << "\nReceived signal " << sig << ". Shutting down..." << std::endl;
}

void RunImpl(const Args& args)
{
	if (args.mode == Args::Mode::Server)
	{
		sigset_t set;

		sigemptyset(&set);
		sigaddset(&set, SIGINT);
		sigaddset(&set, SIGTERM);

		pthread_sigmask(SIG_BLOCK, &set, nullptr);
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
	else if (args.mode == Args::Mode::SingleClient)
	{
		Client client(args.address, args.port, args.name);
		client.run();
	}
	else if (args.mode == Args::Mode::LoadClient)
	{
		std::cout << "Starting load test: " << args.instanceCount << " clients to "
				  << args.address << ":" << args.port << std::endl;

		std::vector<std::jthread> clients;
		clients.reserve(args.instanceCount);

		for (int i = 0; i < args.instanceCount; ++i)
		{
			clients.emplace_back([&, i](std::stop_token st) {
				std::random_device rd;
				std::mt19937 gen(rd());
				std::uniform_int_distribution<> numDist(0, 100);
				std::uniform_int_distribution<> sleepDist(0, 15);

				std::string clientName = args.name + "_" + std::to_string(i);

				try
				{
					Client client(args.address, args.port, clientName);

					int clientNumber = numDist(gen);
					auto sleepSec = sleepDist(gen);
					client.run(clientNumber, sleepSec);
				}
				catch (const std::exception& e)
				{
					std::cerr << "[Client " << clientName << "] Error: " << e.what() << std::endl;
				}
			});
		}

		std::cout << "All " << args.instanceCount << " clients started." << std::endl;

		for (auto& t : clients)
		{
			if (t.joinable())
				t.join();
		}

		std::cout << "Load test completed." << std::endl;
	}
}

int main(int argc, char** argv)
{
	auto args = ParseArgs(argc, argv);
	if (!args)
	{
		std::cout
			<< "Usage:\n"
			<< "  Server mode:      " << argv[0] << " <port> <name>\n"
			<< "  Single client:    " << argv[0] << " <address> <port> <name>\n"
			<< "  Load test client: " << argv[0] << " <address> <port> <base_name> <count>\n"
			<< std::endl;
		return EXIT_FAILURE;
	}

	try
	{
		RunImpl(args.value());
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}