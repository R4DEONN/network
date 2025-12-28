#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <chrono>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <numeric>
#include <algorithm>
#include <iomanip>

long long get_timestamp_ms()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
}

void parseArguments(int argc, char* argv[], std::string& host, int& port)
{
	if (argc != 3)
	{
		std::cerr << "Usage: ./client <server_host> <server_port>" << std::endl;
		exit(1);
	}
	host = argv[1];
	port = std::stoi(argv[2]);
}

int createUdpClientSocket(const std::string& host, int port, int timeout_sec = 1)
{
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		perror("Socket creation failed");
		exit(1);
	}

	struct timeval timeout;
	timeout.tv_sec = timeout_sec;
	timeout.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
	{
		perror("setsockopt failed");
		close(sockfd);
		exit(1);
	}

	return sockfd;
}

struct sockaddr_in configureServerAddress(const std::string& host, int port)
{
	struct sockaddr_in server_addr{};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0)
	{
		std::cerr << "Invalid server address" << std::endl;
		exit(1);
	}
	return server_addr;
}

bool sendPingAndReceiveReply(int sockfd, const struct sockaddr_in& server_addr,
	int seq, std::vector<double>& rtt_list)
{
	long long timestamp = get_timestamp_ms();
	std::string message = "Ping " + std::to_string(seq) + " " + std::to_string(timestamp);

	auto start_time = std::chrono::high_resolution_clock::now();

	if (sendto(sockfd, message.c_str(), message.length(), 0,
		(const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		std::cerr << "Failed to send packet" << std::endl;
		return false;
	}

	char buffer[1024];
	int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, nullptr, nullptr);

	if (n < 0)
	{
		std::cout << "Request timed out" << std::endl;
		return false;
	}
	else
	{
		auto end_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> rtt = end_time - start_time;
		buffer[n] = '\0';

		std::cout << "Ответ от сервера: " << buffer
				  << ", RTT = " << std::fixed << std::setprecision(3)
				  << rtt.count() << " сек" << std::endl;

		rtt_list.push_back(rtt.count());
		return true;
	}
}

void printStatistics(int packets_sent, int packets_received, const std::vector<double>& rtt_list)
{
	int packets_lost = packets_sent - packets_received;
	double loss_rate = (static_cast<double>(packets_lost) / packets_sent) * 100;

	std::cout << "\n--- Статистика ping ---" << std::endl;
	std::cout << "Отправлено: " << packets_sent
			  << ", Получено: " << packets_received
			  << ", Потеряно: " << packets_lost
			  << " (" << std::fixed << std::setprecision(1) << loss_rate << "%)" << std::endl;

	if (!rtt_list.empty())
	{
		double min_rtt = *std::min_element(rtt_list.begin(), rtt_list.end());
		double max_rtt = *std::max_element(rtt_list.begin(), rtt_list.end());
		double avg_rtt = std::accumulate(rtt_list.begin(), rtt_list.end(), 0.0) / rtt_list.size();

		std::cout << "RTT: мин = " << std::fixed << std::setprecision(3) << min_rtt << "с, "
				  << "макс = " << max_rtt << "с, "
				  << "средн = " << avg_rtt << "с" << std::endl;
	}
}

void runPingClient(int sockfd, const struct sockaddr_in& server_addr, int num_pings = 10)
{
	int packets_sent = 0;
	int packets_received = 0;
	std::vector<double> rtt_list;

	for (int i = 1; i <= num_pings; ++i)
	{
		packets_sent++;
		if (sendPingAndReceiveReply(sockfd, server_addr, i, rtt_list))
		{
			packets_received++;
		}
	}

	printStatistics(packets_sent, packets_received, rtt_list);
}

int main(int argc, char* argv[])
{
	std::string host;
	int port;
	parseArguments(argc, argv, host, port);

	int sockfd = createUdpClientSocket(host, port);
	struct sockaddr_in server_addr = configureServerAddress(host, port);

	std::cout << "Запуск UDP Ping клиента к " << host << ":" << port << std::endl;
	runPingClient(sockfd, server_addr);

	close(sockfd);
	return 0;
}