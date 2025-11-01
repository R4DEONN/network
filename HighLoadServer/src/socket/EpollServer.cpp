#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <vector>
#include <string>
#include "EpollServer.h"

constexpr std::chrono::seconds clientTimeout{ 10 };

EpollServer::EpollServer(unsigned short port, int maxEvents)
	: m_maxEvents(maxEvents), m_stopRequested(false)
{
	m_epollFd = epoll_create1(EPOLL_CLOEXEC);
	if (m_epollFd == -1)
	{
		throw std::runtime_error("epoll_create1 failed: " + std::string(strerror(errno)));
	}

	if (!m_server.bind(port) || !m_server.listen())
	{
		throw std::runtime_error("Failed to bind or listen on server socket");
	}

	int flags = fcntl(m_server.getHandle(), F_GETFL, 0);
	fcntl(m_server.getHandle(), F_SETFL, flags | O_NONBLOCK);

	epoll_event event{};
	event.events = EPOLLIN;
	event.data.fd = m_server.getHandle();
	if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_server.getHandle(), &event) == -1)
	{
		throw std::runtime_error("epoll_ctl(server) failed: " + std::string(strerror(errno)));
	}

	std::cout << "EpollServer listening on " << m_server.getLocalAddress() << std::endl;
}

EpollServer::~EpollServer()
{
	if (m_epollFd != -1)
	{
		close(m_epollFd);
	}
}

void EpollServer::setMessageHandler(MessageHandler handler)
{
	m_onMessage = std::move(handler);
}

void EpollServer::run()
{
	std::vector<epoll_event> events(m_maxEvents);

	while (true)
	{
		if (m_stopRequested && m_clientsInfo.empty())
		{
			break;
		}

		int numEvents = epoll_wait(m_epollFd, events.data(), m_maxEvents, 1000);
		if (numEvents == -1)
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

		for (int i = 0; i < numEvents; ++i)
		{
			int fd = events[i].data.fd;

			if (fd == m_server.getHandle())
			{
				handleNewConnection();
			}
			else if (events[i].events & (EPOLLRDHUP | EPOLLERR))
			{
				std::cout << "Client disconnected: " << fd << std::endl;
				removeClient(fd);
			}
			else if (events[i].events & EPOLLIN)
			{
				handleClientData(fd);
			}
		}
	}
}

void EpollServer::handleNewConnection()
{
	auto client = m_server.accept();
	if (!client || !client->isValid())
	{
		return;
	}

	int clientFd = client->getHandle();

	int flags = fcntl(clientFd, F_GETFL, 0);
	fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);

	epoll_event event{};
	event.events = EPOLLIN | EPOLLRDHUP;
	event.data.fd = clientFd;
	if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, clientFd, &event) == -1)
	{
		std::cerr << "Failed to add client socket to epoll" << std::endl;
		return;
	}

	m_clientsInfo[clientFd] = { std::chrono::steady_clock::now(), std::move(client) };
	std::cout << "Client connected: " << clientFd << std::endl;
}

void EpollServer::handleClientData(int clientFd)
{
	auto it = m_clientsInfo.find(clientFd);
	if (it == m_clientsInfo.end())
	{
		return;
	}

	std::string request = it->second.tcp->receiveString();

	if (request.size() == 0)
	{
		std::cout << "Client closed or recv error: " << clientFd << std::endl;
		removeClient(clientFd);
		return;
	}

	it->second.lastActivity = std::chrono::steady_clock::now();

	if (m_onMessage)
	{
		auto clientPtr = it->second.tcp.get();
		m_threadPool.enqueue([this, request = std::move(request), clientPtr]()
		{
			try
			{
				std::string response = m_onMessage(request);
				clientPtr->sendString(response);
			}
			catch (const std::exception& ex)
			{
				std::cerr << "Error in worker thread: " << ex.what() << std::endl;
			}
		});
	}
}

void EpollServer::removeClient(int clientFd)
{
	epoll_ctl(m_epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
	m_clientsInfo.erase(clientFd);
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

	if (!m_clientsInfo.empty())
	{
		std::cout << "Active clients: " << m_clientsInfo.size()
				  << ". Waiting for them to finish..." << std::endl;
	}
}

void EpollServer::checkTimeouts()
{
	auto now = std::chrono::steady_clock::now();
	std::vector<int> timedOutClients;

	for (const auto& [fd, info]: m_clientsInfo)
	{
		if (now - info.lastActivity > clientTimeout)
		{
			std::cout << "Client " << fd << " timed out (no activity for "
					  << clientTimeout.count() << "s). Closing." << std::endl;
			timedOutClients.push_back(fd);
		}
	}

	for (int fd: timedOutClients)
	{
		removeClient(fd);
	}
}