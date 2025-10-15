#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

class ThreadPool
{
public:
	explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
	~ThreadPool();

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	void enqueue(std::function<void()> task);

private:
	std::vector<std::jthread> m_threads;
	std::queue<std::function<void()>> m_tasks;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::atomic<bool> m_stop{false};
};