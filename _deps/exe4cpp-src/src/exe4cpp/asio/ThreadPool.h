/*
 * Copyright (c) 2018, Automatak LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 * disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef EXE4CPP_ASIO_THREADPOOL_H
#define EXE4CPP_ASIO_THREADPOOL_H

#include <functional>
#include <chrono>
#include <thread>

#include "asio.hpp"

namespace exe4cpp
{

/**
*	A thread pool that calls asio::io_service::run
*/
class ThreadPool
{
public:
    using thread_init_t = std::function<void(uint32_t)>;

    ThreadPool(
        const std::shared_ptr<asio::io_service>& io_service,
        uint32_t concurrency
    ) : ThreadPool(
            io_service,
            concurrency,
            [](uint32_t) {},
            [](uint32_t) {}
    )
    {}

    ThreadPool(
        const std::shared_ptr<asio::io_service>& io_service,
        uint32_t concurrency,
        const thread_init_t& on_thread_start,
        const thread_init_t& on_thread_exit
    ) : io_service{io_service},
        on_thread_start{on_thread_start},
        on_thread_exit{on_thread_exit},
        executor_guard{io_service->get_executor()}
    {
        if (concurrency == 0)
        {
            concurrency = 1;
        }

        for (uint32_t i = 0; i < concurrency; ++i)
        {
            auto run = [this, i]()
            {
                this->run(i);
            };
            threads.push_back(std::make_unique<std::thread>(run));
        }
    }

    virtual ~ThreadPool()
    {
        this->shutdown();
        threads.clear();
    }

    void shutdown()
    {
        if (!this->is_shutdown)
        {
            this->is_shutdown = true;
            executor_guard.reset();
            for (auto& thread : threads)
            {
                thread->join();
            }
        }
    }

private:
    void run(uint32_t threadnum)
    {
        this->on_thread_start(threadnum);

        this->io_service->run();

        this->on_thread_exit(threadnum);
    }

    const std::shared_ptr<asio::io_service> io_service;

    thread_init_t on_thread_start;
    thread_init_t on_thread_exit;

    bool is_shutdown = false;

    asio::executor_work_guard<asio::io_context::executor_type> executor_guard;
    std::vector<std::unique_ptr<std::thread>> threads;
};

}

#endif
