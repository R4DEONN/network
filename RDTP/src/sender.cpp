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

void log(const std::string& message) {
	if (debug_mode) {
		std::cout << "[DEBUG] " << message << std::endl;
	}
}

int main(int argc, char* argv[]) {
	if (argc < 4) {
		std::cerr << "Usage: ./rdt_sender <receiver_host> <receiver_port> <file.txt> [-d]" << std::endl;
		return 1;
	}

	std::string host = argv[1];
	int port = std::stoi(argv[2]);
	std::string filename = argv[3];
	if (argc > 4 && std::string(argv[4]) == "-d") {
		debug_mode = true;
	}

	std::ifstream input_file(filename, std::ios::binary);
	if (!input_file.is_open()) {
		std::cerr << "Failed to open input file: " << filename << std::endl;
		return 1;
	}

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror("socket creation failed");
		return 1;
	}

	struct sockaddr_in receiver_addr;
	memset(&receiver_addr, 0, sizeof(receiver_addr));
	receiver_addr.sin_family = AF_INET;
	receiver_addr.sin_port = htons(port);
	inet_pton(AF_INET, host.c_str(), &receiver_addr.sin_addr);

	std::vector<RdtpPacket> all_packets;
	uint32_t seq_counter = 0;
	while (!input_file.eof()) {
		RdtpPacket packet;
		memset(&packet, 0, sizeof(RdtpPacket));
		packet.seq_num = seq_counter++;
		packet.flags = FLAG_DATA;
		input_file.read(packet.data, DATA_SIZE);
		packet.checksum = 0;
		packet.checksum = calculate_checksum(packet);
		all_packets.push_back(packet);
	}

	uint32_t base = 0;
	uint32_t next_seq_num = 0;
	long long total_bytes_sent = 0;
	long long retransmissions = 0;
	long long start_time = get_current_time_ms();

	std::cout << "Starting to send " << filename << " (" << all_packets.size() << " packets)..." << std::endl;

	while (base < all_packets.size()) {
		while (next_seq_num < base + WINDOW_SIZE && next_seq_num < all_packets.size()) {
			sendto(sockfd, &all_packets[next_seq_num], sizeof(RdtpPacket), 0, (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));
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

		if (activity == 0) {
			log("Timeout occurred. Resending window starting from base: " + std::to_string(base));
			retransmissions += (next_seq_num - base);
			for (uint32_t i = base; i < next_seq_num; ++i) {
				sendto(sockfd, &all_packets[i], sizeof(RdtpPacket), 0, (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));
				log("Re-sent packet with seq_num: " + std::to_string(i));
				total_bytes_sent += sizeof(RdtpPacket);
			}
		} else if (activity > 0) {
			RdtpPacket ack_packet;
			recvfrom(sockfd, &ack_packet, sizeof(RdtpPacket), 0, nullptr, nullptr);

			uint16_t received_checksum = ack_packet.checksum;
			ack_packet.checksum = 0;

			if (calculate_checksum(ack_packet) == received_checksum && ack_packet.flags == FLAG_ACK) {
				log("Received ACK for seq_num: " + std::to_string(ack_packet.ack_num));
				base = ack_packet.ack_num + 1;
				log("Window base is now: " + std::to_string(base));
			} else {
				log("Corrupted or non-ACK packet received. Ignoring.");
			}
		}
	}

	RdtpPacket fin_packet;
	memset(&fin_packet, 0, sizeof(RdtpPacket));
	fin_packet.flags = FLAG_FIN;
	fin_packet.checksum = 0;
	fin_packet.checksum = calculate_checksum(fin_packet);
	for (int i=0; i<3; ++i) {
		sendto(sockfd, &fin_packet, sizeof(RdtpPacket), 0, (const struct sockaddr*)&receiver_addr, sizeof(receiver_addr));
	}

	long long end_time = get_current_time_ms();
	double duration_sec = (end_time - start_time) / 1000.0;
	input_file.clear();
	input_file.seekg(0, std::ios::end);
	long long file_size = input_file.tellg();

	std::cout << "\n--- Transfer Statistics ---" << std::endl;
	std::cout << "File size: " << file_size / 1024.0 << " KB" << std::endl;
	std::cout << "Total time: " << duration_sec << " seconds" << std::endl;
	std::cout << "Throughput: " << (file_size / 1024.0) / duration_sec << " KB/s" << std::endl;
	std::cout << "Total packets sent (including retransmissions): " << next_seq_num + retransmissions << std::endl;
	std::cout << "Retransmitted packets: " << retransmissions << std::endl;

	close(sockfd);
	input_file.close();

	return 0;
}