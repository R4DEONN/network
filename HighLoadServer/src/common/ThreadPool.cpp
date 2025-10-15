#include "ThreadPool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t numThreads)
{
	if (numThreads == 0)
	{
		numThreads = 1;
	}

	m_threads.reserve(numThreads);
	for (size_t i = 0; i < numThreads; ++i)
	{
		m_threads.emplace_back([this] {
			while (true)
			{
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(m_mutex);
					m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });

					if (m_stop && m_tasks.empty())
					{
						break;
					}

					task = std::move(m_tasks.front());
					m_tasks.pop();
				}
				if (task)
				{
					task();
				}
			}
		});
	}
}

ThreadPool::~ThreadPool()
{
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_stop = true;
	}
	m_cv.notify_all();
}

void ThreadPool::enqueue(std::function<void()> task)
{
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_stop)
		{
			return;
		}
		m_tasks.push(std::move(task));
	}
	m_cv.notify_one();
}