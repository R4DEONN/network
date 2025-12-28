#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <filesystem>
#include <cstring>

const int BUFFER_SIZE = 8192;
const std::string CACHE_DIR = "cache/";

std::string url_to_filename(const std::string& url)
{
	std::string filename = url;
	std::replace(filename.begin(), filename.end(), '/', '_');
	std::replace(filename.begin(), filename.end(), ':', '_');
	std::replace(filename.begin(), filename.end(), '?', '_');
	std::replace(filename.begin(), filename.end(), '&', '_');
	return CACHE_DIR + filename;
}

std::pair<std::string, std::string> parse_url(const std::string& url)
{
	std::string temp_url = url;
	if (temp_url.rfind("http://", 0) == 0)
	{
		temp_url = temp_url.substr(7);
	}
	size_t path_pos = temp_url.find('/');
	std::string host = (path_pos == std::string::npos) ? temp_url : temp_url.substr(0, path_pos);
	std::string path = (path_pos == std::string::npos) ? "/" : temp_url.substr(path_pos);
	return { host, path };
}

int connect_to_remote_server(const std::string& host)
{
	struct hostent* server = gethostbyname(host.c_str());
	if (!server)
	{
		std::cerr << "Could not resolve host: " << host << std::endl;
		return -1;
	}

	int remote_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (remote_socket < 0)
	{
		perror("Socket creation failed");
		return -1;
	}

	struct sockaddr_in remote_addr{};
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(80);
	memcpy(&remote_addr.sin_addr.s_addr, server->h_addr, server->h_length);

	if (connect(remote_socket, (struct sockaddr*)&remote_addr, sizeof(remote_addr)) < 0)
	{
		perror("Failed to connect to remote server");
		close(remote_socket);
		return -1;
	}

	return remote_socket;
}

void send_http_request(int remote_socket, const std::string& path, const std::string& host, const std::string& http_version)
{
	std::string request = "GET " + path + " " + http_version + "\r\n"
						  + "Host: " + host + "\r\n"
						  + "Connection: close\r\n\r\n";
	send(remote_socket, request.c_str(), request.length(), 0);
}

void relay_response_and_cache(int remote_socket, int client_socket, const std::string& cache_filepath)
{
	std::ofstream cache_file(cache_filepath, std::ios::binary);
	char buffer[BUFFER_SIZE];
	int bytes_received;

	while ((bytes_received = recv(remote_socket, buffer, BUFFER_SIZE, 0)) > 0)
	{
		send(client_socket, buffer, bytes_received, 0);
		if (cache_file.is_open())
		{
			cache_file.write(buffer, bytes_received);
		}
	}

	if (cache_file.is_open())
	{
		cache_file.close();
	}
}

bool serve_from_cache(int client_socket, const std::string& cache_filepath)
{
	std::ifstream cache_file(cache_filepath, std::ios::binary);
	if (!cache_file.is_open())
	{
		return false;
	}

	std::cout << "[INFO] Cache HIT\n";
	char buffer[BUFFER_SIZE];
	while (cache_file.read(buffer, BUFFER_SIZE) || cache_file.gcount() > 0)
	{
		send(client_socket, buffer, cache_file.gcount(), 0);
	}
	cache_file.close();
	return true;
}

void handle_client(int client_socket)
{
	char buffer[BUFFER_SIZE];
	int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
	if (bytes_received <= 0)
	{
		close(client_socket);
		return;
	}
	buffer[bytes_received] = '\0';
	std::string request(buffer);

	std::cout << "--- Received Request ---\n" << request << "\n------------------------\n";

	std::istringstream request_stream(request);
	std::string method, url, http_version;
	request_stream >> method >> url >> http_version;

	if (url.starts_with("/"))
	{
		url = "http://" + url.substr(1);
	}

	if (method != "GET")
	{
		std::cerr << "Unsupported method: " << method << std::endl;
		close(client_socket);
		return;
	}

	std::string cache_filepath = url_to_filename(url);

	if (serve_from_cache(client_socket, cache_filepath))
	{
		close(client_socket);
		return;
	}

	std::cout << "[INFO] Cache MISS for URL: " << url << std::endl;

	auto [host, path] = parse_url(url);
	int remote_socket = connect_to_remote_server(host);
	if (remote_socket < 0)
	{
		close(client_socket);
		return;
	}

	send_http_request(remote_socket, path, host, http_version);
	relay_response_and_cache(remote_socket, client_socket, cache_filepath);

	close(remote_socket);
	close(client_socket);
	std::cout << "[INFO] Client connection closed.\n" << std::endl;
}

void init_cache_dir()
{
	if (!std::filesystem::exists(CACHE_DIR))
	{
		std::filesystem::create_directory(CACHE_DIR);
	}
}

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		std::cerr << "Usage: ./proxy_server <port>" << std::endl;
		return 1;
	}
	int port = std::stoi(argv[1]);

	init_cache_dir();

	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0)
	{
		perror("Socket creation failed");
		return 1;
	}

	struct sockaddr_in server_addr{};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Bind failed");
		close(server_socket);
		return 1;
	}

	if (listen(server_socket, 10) < 0)
	{
		perror("Listen failed");
		close(server_socket);
		return 1;
	}

	std::cout << "HTTP Proxy server is listening on port " << port << "..." << std::endl;

	while (true)
	{
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
		if (client_socket < 0)
		{
			perror("Accept failed");
			continue;
		}
		std::cout << "[INFO] Accepted new connection from " << inet_ntoa(client_addr.sin_addr) << std::endl;
		handle_client(client_socket);
	}

	close(server_socket);
	return 0;
}