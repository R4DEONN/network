#pragma once

#include "TcpServer.h"
#include "../common/ThreadPool.h"
#include <sys/epoll.h>
#include <unordered_map>
#include <functional>
#include <memory>

class EpollServer
{
public:
	using MessageHandler = std::function<std::string(const std::string&)>;

	explicit EpollServer(unsigned short port, int max_events = 64);
	~EpollServer();

	void setMessageHandler(MessageHandler handler);
	void run();
	void shutdown();
	void checkTimeouts();

	std::string getLocalAddress() const;

private:
	void removeClient(int client_fd);
	void handleNewConnection();
	void handleClientData(int clientFd);

	TcpServer m_server;
	int m_epollFd = -1;
	int m_maxEvents;
	MessageHandler m_onMessage;

	struct ClientInfo {
		std::chrono::steady_clock::time_point lastActivity;
		std::unique_ptr<TcpClient> tcp;
	};
	std::unordered_map<int, ClientInfo> m_clientsInfo;

	ThreadPool m_threadPool;

	std::atomic<bool> m_stopRequested = false;
};