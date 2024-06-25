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
#include "catch.hpp"

#include "exe4cpp/asio/ThreadPool.h"
#include "exe4cpp/asio/StrandExecutor.h"

using namespace std;
using namespace std::chrono;
using namespace exe4cpp;

#define SUITE(name) "StrandExecutorTestSuite - " name

TEST_CASE(SUITE("automatically reclaims resources"))
{
    const int NUM_THREAD = 10;
    const int NUM_STRAND = 100;
    const int NUM_OPS = 1000;

    uint32_t counters[NUM_STRAND] = { 0 };

    const auto io_service = std::make_shared<asio::io_service>();

    ThreadPool pool(io_service, NUM_THREAD);

    for (int i = 0; i < NUM_STRAND; ++i)
    {
        auto& counter = counters[i];
        auto exe = StrandExecutor::create(io_service);

        auto increment = [&]()
        {
            ++counter;
        };

        for (int i = 0; i < NUM_OPS; ++i)
        {
            exe->post(increment);
            exe->start(std::chrono::milliseconds(0), increment);
        }
    }

    pool.shutdown();

    for (int i = 0; i < NUM_STRAND; ++i)
    {
        REQUIRE(counters[i] == 2 * NUM_OPS);
    }
}

TEST_CASE(SUITE("executor dispatch is from only one thread at a time"))
{
    const int NUM_THREAD = 10;
    const int NUM_OPS = 1000;

    const auto io_service = std::make_shared<asio::io_service>();

    int sum = 0;

    {
        ThreadPool pool(io_service, NUM_THREAD);
        const auto exe = StrandExecutor::create(io_service);

        for (int i = 0; i < NUM_OPS; ++i)
        {
            auto increment = [&sum]()
            {
                ++sum;
            };
            exe->post(increment);
        }

    } // pool stops when it goes out of scope

    REQUIRE(sum == NUM_OPS);
}

TEST_CASE(SUITE("Executor dispatch is in same order as post order"))
{
    const int NUM_THREAD = 10;
    const int NUM_OPS = 1000;

    const auto io_service = std::make_shared<asio::io_service>();

    int order = 0;
    bool is_ordered = true;

    {
        ThreadPool pool(io_service, NUM_THREAD);
        const auto exe = StrandExecutor::create(io_service);

        for (int i = 0; i < NUM_OPS; ++i)
        {
            auto test_order = [i, &order, &is_ordered]()
            {
                if (is_ordered)
                {
                    if (i == order)
                    {
                        ++order;
                    }
                    else
                    {
                        is_ordered = false;
                    }
                }
            };
            exe->post(test_order);
        }
    }

    REQUIRE(is_ordered);
}

