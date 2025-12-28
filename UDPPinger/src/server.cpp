#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <arpa/inet.h>
#include <unistd.h>

const double LOSS_RATE = 0.3;

int parsePort(int argc, char* argv[])
{
	if (argc != 2)
	{
		std::cerr << "Usage: ./server <port>" << std::endl;
		exit(1);
	}
	return std::stoi(argv[1]);
}

int createUdpServerSocket(int port)
{
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		perror("Socket creation failed");
		exit(1);
	}

	struct sockaddr_in server_addr;
	std::memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	if (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Bind failed");
		close(sockfd);
		exit(1);
	}

	return sockfd;
}

bool shouldDropPacket(double lossRate)
{
	return (static_cast<double>(std::rand()) / RAND_MAX) < lossRate;
}

void handlePacket(int sockfd, char* buffer, size_t bufferSize,
	struct sockaddr_in& client_addr, socklen_t addrLen, double lossRate)
{
	int n = recvfrom(sockfd, buffer, bufferSize - 1, 0,
		(struct sockaddr*)&client_addr, &addrLen);
	if (n < 0)
	{
		perror("recvfrom error");
		return;
	}
	buffer[n] = '\0';

	if (shouldDropPacket(lossRate))
	{
		std::cout << "Packet from " << inet_ntoa(client_addr.sin_addr) << " lost." << std::endl;
		return;
	}

	std::cout << "Received: " << buffer << ". Sending echo." << std::endl;
	sendto(sockfd, buffer, n, 0, (const struct sockaddr*)&client_addr, addrLen);
}

void runPingerServer(int sockfd, double lossRate)
{
	char buffer[1024];
	struct sockaddr_in client_addr;
	socklen_t len = sizeof(client_addr);

	while (true)
	{
		handlePacket(sockfd, buffer, sizeof(buffer), client_addr, len, lossRate);
	}
}

int main(int argc, char* argv[])
{
	int port = parsePort(argc, argv);
	int sockfd = createUdpServerSocket(port);

	std::cout << "UDP Pinger Server is listening on port " << port << "..." << std::endl;

	std::srand(static_cast<unsigned int>(std::time(nullptr)));
	runPingerServer(sockfd, LOSS_RATE);

	close(sockfd);
	return 0;
}