#pragma once

#include <cstdint>
#include <chrono>

const int WINDOW_SIZE = 10;
const int DATA_SIZE = 1024;
const int TIMEOUT_MS = 500;

enum PacketFlags {
	FLAG_DATA = 0,
	FLAG_ACK = 1,
	FLAG_FIN = 2
};

#pragma pack(push, 1)
struct RdtpPacket {
	uint32_t seq_num;
	uint32_t ack_num;
	uint16_t checksum;
	uint16_t flags;
	char data[DATA_SIZE];
};
#pragma pack(pop)

uint16_t calculate_checksum(const RdtpPacket& packet) {
	uint16_t checksum = 0;
	checksum ^= packet.seq_num;
	checksum ^= packet.ack_num;
	checksum ^= packet.flags;
	for (int i = 0; i < DATA_SIZE; ++i) {
		checksum ^= (uint16_t)(packet.data[i]);
	}
	return checksum;
}

long long get_current_time_ms() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}