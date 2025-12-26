#include "rdtp.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

bool debug_mode = false;

void log(const std::string& message) {
	if (debug_mode) {
		std::cout << "[DEBUG] " << message << std::endl;
	}
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		std::cerr << "Usage: ./rdt_receiver <receiver_port> <received_file.txt> [-d]" << std::endl;
		return 1;
	}

	int port = std::stoi(argv[1]);
	std::string output_filename = argv[2];
	if (argc > 3 && std::string(argv[3]) == "-d") {
		debug_mode = true;
	}

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror("socket creation failed");
		return 1;
	}

	struct sockaddr_in receiver_addr, sender_addr;
	memset(&receiver_addr, 0, sizeof(receiver_addr));
	receiver_addr.sin_family = AF_INET;
	receiver_addr.sin_addr.s_addr = INADDR_ANY;
	receiver_addr.sin_port = htons(port);

	if (bind(sockfd, (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr)) < 0) {
		perror("bind failed");
		close(sockfd);
		return 1;
	}

	std::cout << "Receiver is listening on port " << port << "..." << std::endl;
	std::ofstream output_file(output_filename, std::ios::binary);
	if (!output_file.is_open()) {
		std::cerr << "Failed to open output file: " << output_filename << std::endl;
		close(sockfd);
		return 1;
	}

	uint32_t expected_seq_num = 0;
	RdtpPacket received_packet;
	RdtpPacket ack_packet;
	socklen_t sender_len = sizeof(sender_addr);

	while (true) {
		int n = recvfrom(sockfd, &received_packet, sizeof(RdtpPacket), 0, (struct sockaddr*)&sender_addr, &sender_len);
		if (n <= 0) continue;

		uint16_t received_checksum = received_packet.checksum;
		received_packet.checksum = 0;
		if (calculate_checksum(received_packet) != received_checksum) {
			log("Corrupted packet received. Discarding.");
			continue;
		}

		if (received_packet.flags == FLAG_FIN) {
			log("FIN packet received. Closing connection.");
			break;
		}

		log("Received packet with seq_num: " + std::to_string(received_packet.seq_num));

		if (received_packet.seq_num == expected_seq_num) {
			output_file.write(received_packet.data, n - offsetof(RdtpPacket, data));
			log("Packet " + std::to_string(expected_seq_num) + " is correct. Sending ACK.");

			ack_packet.ack_num = expected_seq_num;
			ack_packet.flags = FLAG_ACK;
			ack_packet.checksum = 0;
			ack_packet.checksum = calculate_checksum(ack_packet);
			sendto(sockfd, &ack_packet, sizeof(RdtpPacket), 0, (const struct sockaddr*)&sender_addr, sender_len);

			expected_seq_num++;
		} else {
			log("Out-of-order packet. Expected: " + std::to_string(expected_seq_num) + ". Resending last ACK.");
			ack_packet.ack_num = expected_seq_num - 1;
			ack_packet.flags = FLAG_ACK;
			ack_packet.checksum = 0;
			ack_packet.checksum = calculate_checksum(ack_packet);
			sendto(sockfd, &ack_packet, sizeof(RdtpPacket), 0, (const struct sockaddr*)&sender_addr, sender_len);
		}
	}

	output_file.close();
	close(sockfd);
	std::cout << "File transfer complete. Saved to " << output_filename << std::endl;

	return 0;
}