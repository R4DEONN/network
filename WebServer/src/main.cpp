#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <csignal>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

volatile std::sig_atomic_t g_running = 1;

void signal_handler(int sig)
{
	if (sig == SIGINT)
	{
		std::cout << "\n[INFO] Received SIGINT. Shutting down gracefully...\n";
		g_running = 0;
	}
}

void set_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) return;
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

std::string get_content_type(const std::string& path)
{
	static const std::unordered_map<std::string, std::string> mime_types = {
		{ ".html", "text/html" },
		{ ".htm",  "text/html" },
		{ ".css",  "text/css" },
		{ ".js",   "application/javascript" },
		{ ".png",  "image/png" },
		{ ".jpg",  "image/jpeg" },
		{ ".jpeg", "image/jpeg" },
		{ ".gif",  "image/gif" },
		{ ".ico",  "image/x-icon" }
	};

	size_t pos = path.find_last_of('.');
	if (pos != std::string::npos)
	{
		std::string ext = path.substr(pos);
		auto it = mime_types.find(ext);
		if (it != mime_types.end())
		{
			return it->second;
		}
	}
	return "application/octet-stream";
}

std::string read_file(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary);
	if (!file) return "";
	return std::string(
		(std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>()
	);
}

std::string parse_requested_path(const std::string& request)
{
	if (request.empty() || request.substr(0, 4) != "GET ") return "";

	size_t start = 4;
	size_t end = request.find(' ', start);
	if (end == std::string::npos) return "";

	std::string path = request.substr(start, end - start);
	if (path.empty() || path[0] != '/') return "";

	if (path.find("..") != std::string::npos) return "";

	return path;
}

void send_http_response(int client_fd, int status_code, const std::string& content_type, const std::string& body)
{
	std::string status_line;
	if (status_code == 200)
	{
		status_line = "HTTP/1.1 200 OK\r\n";
	}
	else if (status_code == 400)
	{
		status_line = "HTTP/1.1 400 Bad Request\r\n";
	}
	else if (status_code == 404)
	{
		status_line = "HTTP/1.1 404 Not Found\r\n";
	}
	else
	{
		status_line = "HTTP/1.1 500 Internal Server Error\r\n";
	}

	std::string headers =
		"Content-Type: " + content_type + "\r\n"
										  "Content-Length: " + std::to_string(body.size()) + "\r\n"
																							 "Connection: close\r\n"
																							 "\r\n";

	std::string response = status_line + headers + body;
	write(client_fd, response.c_str(), response.size());
}

void handle_client_request(int client_fd)
{
	char buffer[4096];
	ssize_t n = read(client_fd, buffer, sizeof(buffer));
	if (n <= 0) return;

	std::string request(buffer, n);
	std::string path = parse_requested_path(request);

	if (path.empty())
	{
		send_http_response(client_fd, 400, "text/plain", "Bad Request");
		return;
	}

	if (path == "/") path = "/index.html";
	std::string filepath = "." + path;

	std::string content = read_file(filepath);
	if (!content.empty())
	{
		std::string content_type = get_content_type(filepath);
		send_http_response(client_fd, 200, content_type, content);
		std::cout << "[200] " << path << '\n';
	}
	else
	{
		send_http_response(client_fd, 404, "text/plain", "File Not Found");
		std::cout << "[404] " << path << '\n';
	}
}

int create_and_bind_socket(int port)
{
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		perror("socket");
		return -1;
	}

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		perror("setsockopt");
		close(server_fd);
		return -1;
	}

	struct sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
	{
		perror("bind");
		close(server_fd);
		return -1;
	}

	set_nonblocking(server_fd);

	if (listen(server_fd, 128) == -1)
	{
		perror("listen");
		close(server_fd);
		return -1;
	}

	return server_fd;
}

int main()
{
	const int PORT = 8888;
	const int MAX_EVENTS = 64;

	std::signal(SIGINT, signal_handler);

	int server_fd = create_and_bind_socket(PORT);
	if (server_fd == -1)
	{
		std::cerr << "[ERROR] Failed to create server socket.\n";
		return 1;
	}

	int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1)
	{
		perror("epoll_create1");
		close(server_fd);
		return 1;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = server_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1)
	{
		perror("epoll_ctl: server");
		close(server_fd);
		close(epoll_fd);
		return 1;
	}

	std::cout << "[INFO] Server started on http://localhost:" << PORT << "\n";
	std::cout << "[INFO] Press Ctrl+C to stop.\n";

	struct epoll_event events[MAX_EVENTS];

	while (g_running)
	{
		int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
		if (nfds == -1)
		{
			if (errno == EINTR) continue;
			perror("epoll_wait");
			break;
		}

		for (int i = 0; i < nfds; ++i)
		{
			int fd = events[i].data.fd;

			if (fd == server_fd)
			{
				struct sockaddr_in client_addr;
				socklen_t client_len = sizeof(client_addr);
				int client_fd = accept4(server_fd, (struct sockaddr*)&client_addr,
					&client_len, SOCK_NONBLOCK);
				if (client_fd == -1) continue;

				struct epoll_event client_ev{};
				client_ev.events = EPOLLIN | EPOLLRDHUP;
				client_ev.data.fd = client_fd;
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);
			}
			else
			{
				handle_client_request(fd);
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
				close(fd);
			}
		}
	}

	close(epoll_fd);
	close(server_fd);
	std::cout << "[INFO] Server stopped.\n";
	return 0;
}