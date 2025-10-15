#include "EpollServer.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

constexpr size_t BUFFER_SIZE = 4096;
constexpr std::chrono::seconds TIMEOUT{5};

EpollServer::EpollServer(unsigned short port, int max_events)
	: m_max_events(max_events)
{
	m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (m_epoll_fd == -1)
	{
		throw std::runtime_error("epoll_create1 failed: " + std::string(strerror(errno)));
	}

	if (!m_server.bind(port) || !m_server.listen())
	{
		throw std::runtime_error("Failed to bind/listen server socket");
	}

	int flags = fcntl(m_server.getHandle(), F_GETFL, 0);
	fcntl(m_server.getHandle(), F_SETFL, flags | O_NONBLOCK);

	epoll_event ev{};
	ev.events = EPOLLIN;
	ev.data.fd = m_server.getHandle();
	if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_server.getHandle(), &ev) == -1)
	{
		throw std::runtime_error("epoll_ctl(server) failed: " + std::string(strerror(errno)));
	}

	std::cout << "EpollServer listening on " << m_server.getLocalAddress() << std::endl;
}

EpollServer::~EpollServer()
{
	if (m_epoll_fd != -1)
	{
		close(m_epoll_fd);
	}
}

void EpollServer::setMessageHandler(MessageHandler handler)
{
	m_onMessage = std::move(handler);
}

void EpollServer::run()
{
	std::vector<epoll_event> events(m_max_events);

	while (true)
	{
		if (m_stopRequested && m_clientsInfo.size() == 0)
		{
			break;
		}
		int nfds = epoll_wait(m_epoll_fd, events.data(), m_max_events, 1000);
		if (nfds == -1)
		{
			if (errno == EINTR)
			{
				continue;
			}
			if (!m_stopRequested)
			{
				std::cerr << "epoll_wait error: " << strerror(errno) << std::endl;
			}
			break;
		}

		checkTimeouts();

		for (int i = 0; i < nfds; ++i)
		{
			int fd = events[i].data.fd;

			if (fd == m_server.getHandle())
			{
				auto client = m_server.accept();
				if (client && client->isValid())
				{
					int client_fd = client->getHandle();
					int flags = fcntl(client_fd, F_GETFL, 0);
					fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

					epoll_event ev{};
					ev.events = EPOLLIN | EPOLLRDHUP;
					ev.data.fd = client_fd;
					if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
					{
						std::cerr << "Failed to add client to epoll" << std::endl;
					}
					else
					{
						m_clientsInfo[client_fd] = {std::chrono::steady_clock::now(), std::move(client)};
						std::cout << "Client connected: " << client_fd << std::endl;
					}
				}
			}
			else if (events[i].events & (EPOLLRDHUP | EPOLLERR))
			{
				std::cout << "Client disconnected: " << fd << std::endl;
				removeClient(fd);
			}
			else if (events[i].events & EPOLLIN)
			{
				auto it = m_clientsInfo.find(fd);
				if (it == m_clientsInfo.end()) continue;

				char buffer[BUFFER_SIZE];
				int bytes = it->second.tcp->recv(buffer, BUFFER_SIZE - 1);

				if (bytes <= 0)
				{
					std::cout << "Client closed or error: " << fd << std::endl;
					removeClient(fd);
				}
				else
				{
					buffer[bytes] = '\0';
					std::string request(buffer, bytes);

					m_clientsInfo[fd].lastActivity = std::chrono::steady_clock::now();

					if (m_onMessage)
					{
						auto clientPtr = it->second.tcp.get();

						m_threadPool.enqueue([this, request, clientPtr]() {
							try
							{
								std::string response = m_onMessage(request);
								clientPtr->sendString(response);
							}
							catch (const std::exception& e)
							{
								std::cerr << "Error in worker thread: " << e.what() << std::endl;
							}
						});
					}
				}
			}
		}
	}
}

void EpollServer::removeClient(int client_fd)
{
	m_clientsInfo.erase(client_fd);
	epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
}

std::string EpollServer::getLocalAddress() const
{
	return m_server.getLocalAddress();
}

void EpollServer::shutdown()
{
	m_stopRequested = true;
	m_server.close();
	std::cout << "Server stopped accepting new connections." << std::endl;
	if (m_clientsInfo.size() > 0)
	{
		std::cout << "Active clients: " << m_clientsInfo.size() << ". Waiting for them to finish..." << std::endl;
	}
}

void EpollServer::checkTimeouts()
{
	auto now = std::chrono::steady_clock::now();
	std::vector<int> toRemove;

	for (const auto& [fd, info] : m_clientsInfo)
	{
		if (now - info.lastActivity > TIMEOUT)
		{
			std::cout << "Client " << fd << " timed out (no activity for "
					  << TIMEOUT.count() << "s). Closing." << std::endl;
			toRemove.push_back(fd);
		}
	}

	for (int fd : toRemove)
	{
		removeClient(fd);
	}
}