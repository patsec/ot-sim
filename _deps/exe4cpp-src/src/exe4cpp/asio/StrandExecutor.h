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
#ifndef EXE4CPP_ASIO_STRANDEXECUTOR_H
#define EXE4CPP_ASIO_STRANDEXECUTOR_H

#include "exe4cpp/asio/AsioExecutor.h"
#include "exe4cpp/asio/AsioSystemTimer.h"
#include "exe4cpp/asio/AsioTimer.h"

#include "asio.hpp"

namespace exe4cpp
{

/**
*
* Implementation of openpal::IExecutor backed by asio::strand
*
* Shutdown life-cycle guarantees are provided by using std::shared_ptr
*
*/
class StrandExecutor final :
    public exe4cpp::AsioExecutor,
    public std::enable_shared_from_this<StrandExecutor>
{

public:

    StrandExecutor(const std::shared_ptr<asio::io_context>& io_context) :
        AsioExecutor{io_context},
        strand{*io_context}
    {}

    static std::shared_ptr<StrandExecutor> create(const std::shared_ptr<asio::io_context>& io_context)
    {
        return std::make_shared<StrandExecutor>(io_context);
    }

    std::shared_ptr<StrandExecutor> fork()
    {
        return create(this->io_context);
    }

    // ---- Implement IExecutor -----

    Timer start(const duration_t& duration, const action_t& action) final
    {
        return this->start(get_time() + duration, action);
    }

    Timer start(const steady_time_t& expiration, const action_t& action) final
    {
        const auto timer = AsioTimer::create(this->io_context);

        timer->impl.expires_at(expiration);

        // neither this executor nor the timer can be deleted while the timer is still active
        auto callback = [timer, action, self = shared_from_this()](const std::error_code & ec)
        {
            if (!ec)   // an error indicate timer was canceled
            {
                action();
            }
        };

        timer->impl.async_wait(strand.wrap(callback));

        return Timer(timer);
    }

    Timer start(const system_time_t& expiration, const action_t& action) final
    {
        const auto timer = AsioSystemTimer::create(this->io_context);

        timer->impl.expires_at(expiration);

        // neither this executor nor the timer can be deleted while the timer is still active
        auto callback = [timer, action, self = shared_from_this()](const std::error_code & ec)
        {
            if (!ec)   // an error indicate timer was canceled
            {
                action();
            }
        };

        timer->impl.async_wait(strand.wrap(callback));

        return Timer(timer);
    }

    void post(const action_t& action) final
    {
        auto callback = [action, self = shared_from_this()]()
        {
            action();
        };

        strand.post(callback);
    }

    steady_time_t get_time() final
    {
        return std::chrono::steady_clock::now();
    }

    bool is_running_in_this_thread() final
    {
        return strand.running_in_this_thread();
    }

    template <typename handler_t>
    asio::detail::wrapped_handler<asio::io_context::strand, handler_t, asio::detail::is_continuation_if_running> wrap(const handler_t& handler)
    {
        return strand.wrap(handler);
    }

private:
    asio::io_context::strand strand;
};

}

#endif
