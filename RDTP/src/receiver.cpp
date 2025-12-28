#include "utils.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

bool debug_mode = false;

void log(const std::string& message)
{
	if (debug_mode)
	{
		std::cout << "[DEBUG] " << message << std::endl;
	}
}

struct Config
{
	int port;
	std::string output_filename;
	bool debug;
};

Config parse_args(int argc, char* argv[])
{
	if (argc < 3)
	{
		throw std::invalid_argument("Usage: ./rdt_receiver <receiver_port> <received_file.txt> [-d]");
	}

	Config cfg;
	cfg.port = std::stoi(argv[1]);
	cfg.output_filename = argv[2];
	cfg.debug = (argc > 3 && std::string(argv[3]) == "-d");
	return cfg;
}

void send_ack(int sockfd, const struct sockaddr_in& sender_addr, socklen_t addr_len, uint32_t ack_num)
{
	RdtpPacket ack{};
	ack.ack_num = ack_num;
	ack.flags = FLAG_ACK;
	ack.checksum = calculate_checksum(ack);
	sendto(sockfd, &ack, sizeof(RdtpPacket), 0,
		reinterpret_cast<const struct sockaddr*>(&sender_addr), addr_len);
}

int main(int argc, char* argv[])
{
	try
	{
		Config cfg = parse_args(argc, argv);
		debug_mode = cfg.debug;

		int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sockfd < 0)
		{
			perror("socket creation failed");
			return 1;
		}

		struct sockaddr_in receiver_addr{};
		receiver_addr.sin_family = AF_INET;
		receiver_addr.sin_addr.s_addr = INADDR_ANY;
		receiver_addr.sin_port = htons(cfg.port);

		if (bind(sockfd, reinterpret_cast<const struct sockaddr*>(&receiver_addr), sizeof(receiver_addr)) < 0)
		{
			perror("bind failed");
			close(sockfd);
			return 1;
		}

		std::ofstream output_file(cfg.output_filename, std::ios::binary);
		if (!output_file.is_open())
		{
			throw std::runtime_error("Failed to open output file: " + cfg.output_filename);
		}

		std::cout << "Receiver is listening on port " << cfg.port << "..." << std::endl;

		uint32_t expected_seq_num = 0;
		RdtpPacket received_packet;
		struct sockaddr_in sender_addr;
		socklen_t sender_len = sizeof(sender_addr);

		while (true)
		{
			ssize_t n = recvfrom(sockfd, &received_packet, sizeof(RdtpPacket), 0,
				reinterpret_cast<struct sockaddr*>(&sender_addr), &sender_len);
			if (n <= 0) continue;

			uint16_t received_checksum = received_packet.checksum;
			received_packet.checksum = 0;
			if (calculate_checksum(received_packet) != received_checksum)
			{
				log("Corrupted packet received. Discarding.");
				continue;
			}

			if (received_packet.flags == FLAG_FIN)
			{
				log("FIN packet received. Closing connection.");
				break;
			}

			log("Received packet with seq_num: " + std::to_string(received_packet.seq_num));

			if (received_packet.seq_num == expected_seq_num)
			{
				output_file.write(received_packet.data, received_packet.data_len);
				log("Packet " + std::to_string(expected_seq_num) + " is correct. Sending ACK.");

				send_ack(sockfd, sender_addr, sender_len, expected_seq_num);
				expected_seq_num++;
			}
			else
			{
				log("Out-of-order packet. Expected: " + std::to_string(expected_seq_num) +
					". Resending last ACK for " + std::to_string(expected_seq_num - 1) + ".");
				send_ack(sockfd, sender_addr, sender_len, expected_seq_num - 1);
			}
		}

		output_file.close();
		close(sockfd);
		std::cout << "File transfer complete. Saved to " << cfg.output_filename << std::endl;
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}