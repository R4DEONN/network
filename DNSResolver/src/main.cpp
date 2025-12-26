#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <ctime>
#include <unordered_map>

#ifdef __linux__

#include <sys/select.h>

#endif

constexpr uint16_t DNS_PORT = 53;
constexpr size_t MAX_UDP_SIZE = 512;
constexpr size_t MAX_TCP_SIZE = 65535;

const std::vector<std::string> ROOT_SERVERS = {
	"198.41.0.4",     // a.root-servers.net
	"199.9.14.201",   // b.root-servers.net
	"192.33.4.12",    // c.root-servers.net
	"199.7.91.13",    // d.root-servers.net
	"192.203.230.10", // e.root-servers.net
	"192.5.5.241",    // f.root-servers.net
	"192.112.36.4",   // g.root-servers.net
	"198.97.190.53",  // h.root-servers.net
	"192.36.148.17",  // i.root-servers.net
	"192.58.128.30",  // j.root-servers.net
	"193.0.14.129",   // k.root-servers.net
	"199.7.83.42",    // l.root-servers.net
	"202.12.27.33"    // m.root-servers.net
};

enum class QType : uint16_t
{
	A = 1,
	AAAA = 28,
};

bool debug_mode = false;

void debug_log(const std::string& msg)
{
	if (debug_mode)
	{
		std::cerr << "[DEBUG] " << msg << "\n";
	}
}

std::vector<uint8_t> encode_domain_name(const std::string& domain)
{
	std::vector<uint8_t> out;
	size_t start = 0;
	for (size_t i = 0; i <= domain.size(); ++i)
	{
		if (i == domain.size() || domain[i] == '.')
		{
			uint8_t len = static_cast<uint8_t>(i - start);
			out.push_back(len);
			for (size_t j = start; j < i; ++j)
				out.push_back(static_cast<uint8_t>(std::tolower(domain[j])));
			start = i + 1;
		}
	}
	out.push_back(0);
	return out;
}

std::string decode_domain_name(const uint8_t* data, size_t offset, size_t max_len, size_t& consumed)
{
	std::string out;
	size_t pos = offset;
	while (pos < max_len)
	{
		uint8_t len = data[pos];
		if (len == 0)
		{
			consumed = pos - offset + 1;
			return out;
		}
		if ((len & 0xC0) == 0xC0)
		{
			if (pos + 2 > max_len) break;
			uint16_t ptr = ((len & 0x3F) << 8) | data[pos + 1];
			size_t sub_consumed = 0;
			std::string pointed = decode_domain_name(data, ptr, max_len, sub_consumed);
			if (!out.empty()) out += ".";
			out += pointed;
			consumed = pos - offset + 2;
			return out;
		}
		if (pos + 1 + len > max_len) break;
		if (!out.empty()) out += ".";
		for (size_t i = 0; i < len; ++i)
			out += static_cast<char>(data[pos + 1 + i]);
		pos += 1 + len;
	}
	consumed = pos - offset;
	return out;
}

uint16_t ntohs_uint16(const uint8_t* ptr)
{
	uint16_t val;
	std::memcpy(&val, ptr, sizeof(val));
	return ntohs(val);
}

struct DNSHeader
{
	uint16_t id;
	uint16_t flags;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
};

int create_udp_socket()
{
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		perror("socket(UDP)");
		return -1;
	}
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	return sockfd;
}

int create_tcp_socket()
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("socket(TCP)");
		return -1;
	}
	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	return sockfd;
}

bool send_udp_packet(int sockfd, const struct sockaddr_in& server_addr, const std::vector<uint8_t>& buf)
{
	ssize_t sent = sendto(sockfd, buf.data(), buf.size(), 0,
		reinterpret_cast<const struct sockaddr*>(&server_addr),
		sizeof(server_addr));
	return static_cast<size_t>(sent) == buf.size();
}

bool receive_udp_packet(int sockfd, std::vector<uint8_t>& buf)
{
	buf.resize(MAX_UDP_SIZE);
	ssize_t received = recvfrom(sockfd, buf.data(), buf.size(), 0, nullptr, nullptr);
	if (received <= 0) return false;
	buf.resize(received);
	return true;
}

bool send_tcp_packet(int sockfd, const std::vector<uint8_t>& buf)
{
	uint16_t len = htons(static_cast<uint16_t>(buf.size()));
	std::vector<uint8_t> full;
	full.push_back(static_cast<uint8_t>(len >> 8));
	full.push_back(static_cast<uint8_t>(len & 0xFF));
	full.insert(full.end(), buf.begin(), buf.end());
	ssize_t sent = send(sockfd, full.data(), full.size(), 0);
	return static_cast<size_t>(sent) == full.size();
}

bool receive_tcp_packet(int sockfd, std::vector<uint8_t>& buf)
{
	uint8_t len_bytes[2];
	if (recv(sockfd, len_bytes, 2, 0) != 2) return false;
	uint16_t len = (static_cast<uint16_t>(len_bytes[0]) << 8) | len_bytes[1];
	if (len > MAX_TCP_SIZE) return false;
	buf.resize(len);
	ssize_t received = recv(sockfd, buf.data(), len, 0);
	return static_cast<size_t>(received) == len;
}

std::vector<uint8_t> build_dns_query_packet(uint16_t id, const std::string& qname, QType qtype)
{
	std::vector<uint8_t> packet;
	packet.resize(12, 0);
	uint16_t net_id = htons(id);
	std::memcpy(packet.data(), &net_id, sizeof(net_id));
	packet[2] = 0x00;
	packet[4] = 0x00;
	packet[5] = 0x01;

	auto qn = encode_domain_name(qname);
	packet.insert(packet.end(), qn.begin(), qn.end());

	uint16_t t = static_cast<uint16_t>(qtype);
	packet.push_back(static_cast<uint8_t>(t >> 8));
	packet.push_back(static_cast<uint8_t>(t & 0xFF));

	packet.push_back(0);
	packet.push_back(1);

	return packet;
}

bool is_truncated_flag_set(const std::vector<uint8_t>& response)
{
	if (response.size() < 4) return false;
	uint16_t flags = ntohs_uint16(response.data() + 2);
	return (flags & 0x0200) != 0;
}

std::vector<std::string> parse_dns_response(
	const std::vector<uint8_t>& response,
	QType expected_type,
	std::vector<std::string>& out_next_ns,
	std::unordered_map<std::string, std::string>& out_glue,
	std::string& out_cname  // для CNAME поддержки
)
{
	out_next_ns.clear();
	out_glue.clear();
	out_cname.clear();
	std::vector<std::string> answers;

	if (response.size() < sizeof(DNSHeader)) return answers;

	const DNSHeader* hdr = reinterpret_cast<const DNSHeader*>(response.data());
	uint16_t qdcount = ntohs(hdr->qdcount);
	uint16_t ancount = ntohs(hdr->ancount);
	uint16_t nscount = ntohs(hdr->nscount);
	uint16_t arcount = ntohs(hdr->arcount);

	size_t pos = sizeof(DNSHeader);

	// QUESTION
	for (int i = 0; i < qdcount && pos < response.size(); ++i)
	{
		size_t consumed = 0;
		decode_domain_name(response.data(), pos, response.size(), consumed);
		pos += consumed + 4; // +4 = TYPE (2) + CLASS (2)
	}

	// ANSWER
	for (int i = 0; i < ancount && pos < response.size(); ++i)
	{
		size_t consumed = 0;
		std::string name = decode_domain_name(response.data(), pos, response.size(), consumed);
		pos += consumed;
		if (pos + 10 > response.size()) break;

		uint16_t type = ntohs_uint16(response.data() + pos);
		uint16_t rdlength = ntohs_uint16(response.data() + pos + 8);
		pos += 10;
		if (pos + rdlength > response.size()) break;

		if (type == 5)
		{ // CNAME
			size_t cname_consumed = 0;
			std::string cname = decode_domain_name(response.data(), pos, response.size(), cname_consumed);
			out_cname = cname;
		}
		else if ((expected_type == QType::A && type == 1) ||
				 (expected_type == QType::AAAA && type == 28))
		{
			if ((expected_type == QType::A && rdlength == 4) ||
				(expected_type == QType::AAAA && rdlength == 16))
			{
				char ip_str[INET6_ADDRSTRLEN];
				if (expected_type == QType::A)
					inet_ntop(AF_INET, response.data() + pos, ip_str, sizeof(ip_str));
				else
					inet_ntop(AF_INET6, response.data() + pos, ip_str, sizeof(ip_str));
				answers.emplace_back(ip_str);
			}
		}
		pos += rdlength;
	}

	// AUTHORITY (NS записи)
	for (int i = 0; i < nscount && pos < response.size(); ++i)
	{
		size_t consumed = 0;
		std::string name = decode_domain_name(response.data(), pos, response.size(), consumed);
		pos += consumed;
		if (pos + 10 > response.size()) break;

		uint16_t type = ntohs_uint16(response.data() + pos);
		uint16_t rdlength = ntohs_uint16(response.data() + pos + 8);
		pos += 10;
		if (pos + rdlength > response.size()) break;

		if (type == 2)
		{ // NS
			size_t ns_consumed = 0;
			std::string ns_name = decode_domain_name(response.data(), pos, response.size(), ns_consumed);
			out_next_ns.push_back(ns_name);
		}
		pos += rdlength;
	}

	// ADDITIONAL (glue)
	for (int i = 0; i < arcount && pos < response.size(); ++i)
	{
		size_t consumed = 0;
		std::string name = decode_domain_name(response.data(), pos, response.size(), consumed);
		pos += consumed;
		if (pos + 10 > response.size()) break;

		uint16_t type = ntohs_uint16(response.data() + pos);
		uint16_t rdlength = ntohs_uint16(response.data() + pos + 8);
		pos += 10;
		if (pos + rdlength > response.size()) break;

		if (type == 1 && rdlength == 4)
		{ // A glue
			char ip_str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, response.data() + pos, ip_str, sizeof(ip_str));
			out_glue[name] = std::string(ip_str);
		}
		pos += rdlength;
	}

	return answers;
}

std::vector<std::string> iterative_dns_resolve(const std::string& domain, QType qtype, int depth = 0)
{
	std::vector<std::string> current_servers = ROOT_SERVERS;
	std::string current_qname = domain;

	const int MAX_ITERATIONS = 15;
	int iterations = 0;

	while (!current_servers.empty() && iterations < MAX_ITERATIONS)
	{
		iterations++;
		std::string server = current_servers.back();
		current_servers.pop_back();

		debug_log("Querying " + server + " for " + current_qname);

		std::vector<std::string> next_ns;
		std::unordered_map<std::string, std::string> glue;
		std::string cname_target;
		int sockfd = create_udp_socket();
		if (sockfd < 0) continue;

		struct sockaddr_in server_addr{};
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(DNS_PORT);
		if (inet_pton(AF_INET, server.c_str(), &server_addr.sin_addr) <= 0)
		{
			close(sockfd);
			continue;
		}

		uint16_t id = static_cast<uint16_t>(std::time(nullptr) ^ getpid());
		auto query = build_dns_query_packet(id, current_qname, qtype);
		if (!send_udp_packet(sockfd, server_addr, query))
		{
			close(sockfd);
			continue;
		}

		std::vector<uint8_t> raw_response;
		if (!receive_udp_packet(sockfd, raw_response))
		{
			close(sockfd);
			continue;
		}
		close(sockfd);

		if (raw_response.size() < 2 || ntohs_uint16(raw_response.data()) != id) continue;

		if (is_truncated_flag_set(raw_response))
		{
			debug_log("Create tcp connection");
			int tcp_sock = create_tcp_socket();
			if (tcp_sock >= 0)
			{
				if (connect(tcp_sock, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) == 0)
				{
					if (send_tcp_packet(tcp_sock, query))
					{
						if (!receive_tcp_packet(tcp_sock, raw_response))
						{
							close(tcp_sock);
							continue;
						}
					}
				}
				close(tcp_sock);
			}
		}

		auto answers = parse_dns_response(raw_response, qtype, next_ns, glue, cname_target);

		if (!cname_target.empty() && answers.empty())
		{
			debug_log("Following CNAME: " + cname_target);
			current_qname = cname_target;
			continue;
		}

		if (!answers.empty())
		{
			return answers;
		}

		if (!next_ns.empty())
		{
			current_servers.clear();
			for (const auto& ns: next_ns)
			{
				if (glue.count(ns))
				{
					debug_log("Using glue for " + ns);
					current_servers.push_back(glue[ns]);
				}
				else
				{
					if (depth >= 5) continue;
					auto ns_ips = iterative_dns_resolve(ns, QType::A, depth + 1);
					for (const auto& ip: ns_ips)
					{
						current_servers.push_back(ip);
					}
				}
			}
			if (current_servers.empty()) break;
		}
		else
		{
			break;
		}
	}

	if (iterations >= MAX_ITERATIONS)
	{
		debug_log("→ Too many iterations, aborting to prevent loop");
		return {};
	}

	debug_log("Failed to resolve " + domain);
	return {};
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cerr << "Usage:\n"
				  << "  " << argv[0] << " <domain> <A|AAAA> [-d]\n";
		return 1;
	}

	std::string domain = argv[1];
	std::string rtype_str = (argc >= 3) ? argv[2] : "A";
	QType qtype;

	if (rtype_str == "A")
	{
		qtype = QType::A;
	}
	else if (rtype_str == "AAAA")
	{
		qtype = QType::AAAA;
	}
	else
	{
		std::cerr << "Unsupported record type: " << rtype_str << "\n";
		return 1;
	}

	if (argc >= 4 && std::string(argv[3]) == "-d")
	{
		debug_mode = true;
	}

	auto result = iterative_dns_resolve(domain, qtype);

	if (result.empty())
	{
		if (!debug_mode) std::cout << ";; Not found\n";
		return 1;
	}

	for (const auto& ip: result)
	{
		std::cout << ip << "\n";
	}

	return 0;
}