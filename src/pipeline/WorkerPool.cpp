// WorkerPool.cpp
#include "pipeline/WorkerPool.h"

namespace mdp
{
    WorkerPool::WorkerPool(
        ThreadSafeQueue<MarketDataEvent>& queue,
        EventProcessor& processor,
        std::size_t workerCount)
        : m_queue(queue)
        , m_processor(processor)
        , m_workerCount(workerCount)
    {
    }

    WorkerPool::~WorkerPool()
    {
        stop();
    }

    void WorkerPool::start()
    {
        if (m_running.load())
        {
            return;
        }

        m_running = true;
        m_workerThreads.reserve(m_workerCount);

        for (std::size_t i = 0; i < m_workerCount; ++i)
        {
            m_workerThreads.emplace_back(&WorkerPool::workerLoop, this);
        }
    }

    void WorkerPool::stop()
    {
        if (!m_running.load())
        {
            return;
        }

        m_running = false;

        for (std::thread& workerThread : m_workerThreads)
        {
            if (workerThread.joinable())
            {
                workerThread.join();
            }
        }

        m_workerThreads.clear();
    }

    void WorkerPool::workerLoop()
    {
        MarketDataEvent event;

        while (m_queue.pop(event))
        {
            m_processor.process(event);
        }
    }
}
