#include "utils.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <map>

bool debug_mode = false;

void log(const std::string& message)
{
	if (debug_mode)
	{
		std::cout << "[DEBUG] " << message << std::endl;
	}
}
struct Config {
	std::string host;
	int port;
	std::string filename;
	bool debug;
};

Config parse_args(int argc, char* argv[])
{
	if (argc < 4)
	{
		throw std::invalid_argument("Usage: ./rdt_sender <receiver_host> <receiver_port> <file.txt> [-d]");
	}

	Config cfg;
	cfg.host = argv[1];
	cfg.port = std::stoi(argv[2]);
	cfg.filename = argv[3];
	cfg.debug = (argc > 4 && std::string(argv[4]) == "-d");
	return cfg;
}

std::vector<RdtpPacket> read_file_into_packets(const std::string& filename)
{
	std::ifstream input_file(filename, std::ios::binary);
	if (!input_file.is_open())
	{
		throw std::runtime_error("Failed to open input file: " + filename);
	}

	std::vector<RdtpPacket> packets;
	uint32_t seq_counter = 0;
	while (!input_file.eof())
	{
		RdtpPacket packet{};
		packet.seq_num = seq_counter++;
		packet.flags = FLAG_DATA;
		input_file.read(packet.data, DATA_SIZE);
		packet.data_len = static_cast<uint16_t>(input_file.gcount());
		packet.checksum = calculate_checksum(packet);
		packets.push_back(packet);
	}
	return packets;
}

void send_packet(int sockfd, const RdtpPacket& packet, const struct sockaddr_in& addr)
{
	sendto(sockfd, &packet, sizeof(RdtpPacket), 0,
		reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));
}

bool is_valid_ack(const RdtpPacket& packet)
{
	if (packet.flags != FLAG_ACK) return false;
	uint16_t received_checksum = packet.checksum;
	RdtpPacket copy = packet;
	copy.checksum = 0;
	return calculate_checksum(copy) == received_checksum;
}

void send_fin_packet(int sockfd, const struct sockaddr_in& addr)
{
	RdtpPacket fin_packet{};
	fin_packet.flags = FLAG_FIN;
	fin_packet.checksum = calculate_checksum(fin_packet);
	for (int i = 0; i < 3; ++i)
	{
		send_packet(sockfd, fin_packet, addr);
	}
}

int main(int argc, char* argv[])
{
	try
	{
		Config cfg = parse_args(argc, argv);
		debug_mode = cfg.debug;

		auto all_packets = read_file_into_packets(cfg.filename);

		int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sockfd < 0)
		{
			perror("socket creation failed");
			return 1;
		}

		struct sockaddr_in receiver_addr{};
		receiver_addr.sin_family = AF_INET;
		receiver_addr.sin_port = htons(cfg.port);
		inet_pton(AF_INET, cfg.host.c_str(), &receiver_addr.sin_addr);

		uint32_t base = 0;
		uint32_t next_seq_num = 0;
		long long total_bytes_sent = 0;
		long long retransmissions = 0;
		long long start_time = get_current_time_ms();

		std::cout << "Starting to send " << cfg.filename << " (" << all_packets.size() << " packets)..." << std::endl;

		while (base < all_packets.size())
		{
			while (next_seq_num < base + WINDOW_SIZE && next_seq_num < all_packets.size())
			{
				send_packet(sockfd, all_packets[next_seq_num], receiver_addr);
				log("Sent packet with seq_num: " + std::to_string(next_seq_num));
				total_bytes_sent += sizeof(RdtpPacket);
				next_seq_num++;
			}

			fd_set read_fds;
			FD_ZERO(&read_fds);
			FD_SET(sockfd, &read_fds);

			struct timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = TIMEOUT_MS * 1000;

			int activity = select(sockfd + 1, &read_fds, nullptr, nullptr, &timeout);

			if (activity == 0)
			{
				log("Timeout occurred. Resending window starting from base: " + std::to_string(base));
				retransmissions += (next_seq_num - base);
				for (uint32_t i = base; i < next_seq_num; ++i)
				{
					send_packet(sockfd, all_packets[i], receiver_addr);
					log("Re-sent packet with seq_num: " + std::to_string(i));
					total_bytes_sent += sizeof(RdtpPacket);
				}
			}
			else if (activity > 0)
			{
				RdtpPacket ack_packet;
				recvfrom(sockfd, &ack_packet, sizeof(RdtpPacket), 0, nullptr, nullptr);

				if (is_valid_ack(ack_packet))
				{
					log("Received ACK for seq_num: " + std::to_string(ack_packet.ack_num));
					base = ack_packet.ack_num + 1;
					log("Window base is now: " + std::to_string(base));
				}
				else
				{
					log("Corrupted or non-ACK packet received. Ignoring.");
				}
			}
		}

		send_fin_packet(sockfd, receiver_addr);

		long long end_time = get_current_time_ms();
		double duration_sec = (end_time - start_time) / 1000.0;

		std::ifstream input_file(cfg.filename, std::ios::binary);
		input_file.seekg(0, std::ios::end);
		long long file_size = input_file.tellg();
		input_file.close();

		std::cout << "\n--- Transfer Statistics ---" << std::endl;
		std::cout << "File size: " << file_size / 1024.0 << " KB" << std::endl;
		std::cout << "Total time: " << duration_sec << " seconds" << std::endl;
		std::cout << "Throughput: " << (file_size / 1024.0) / duration_sec << " KB/s" << std::endl;
		std::cout << "Total packets sent (including retransmissions): " << next_seq_num + retransmissions << std::endl;
		std::cout << "Retransmitted packets: " << retransmissions << std::endl;

		close(sockfd);
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}