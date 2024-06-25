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
#ifndef EXE4CPP_ASIO_ASIOEXECUTOR_H
#define EXE4CPP_ASIO_ASIOEXECUTOR_H

#include "exe4cpp/IExecutor.h"

#include "asio.hpp"

namespace exe4cpp
{

/**
*
* Simple implementation of openpal::IExecutor that directly uses asio::io_context
*
* Should only be used when asio::io_context::run() is called from a single thread
*
*/
class AsioExecutor :
    public exe4cpp::IExecutor
{
public:
    AsioExecutor(const std::shared_ptr<asio::io_context>& io_context) : io_context{io_context}
    {}

    virtual ~AsioExecutor() = default;

    // Uncopyable
    AsioExecutor(const AsioExecutor&) = delete;
    AsioExecutor& operator=(const AsioExecutor&) = delete;

    // lots of ASIO components must be initialized with a reference to the io_context
    inline std::shared_ptr<asio::io_context> get_context()
    {
        return io_context;
    }

protected:
    // we hold a shared_ptr to the io_context so that it cannot dissapear while the executor is still around
    const std::shared_ptr<asio::io_context> io_context;
};

}

#endif
