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

#include "ser4cpp/container/StaticBuffer.h"

using namespace ser4cpp;

#define SUITE(name) "StaticBuffer - " name

TEST_CASE(SUITE("Static buffers can be copied"))
{
    StaticBuffer<4> buffer;

    {
        auto dest = buffer.as_wseq();
        dest[0] = 0xDE;
        dest[1] = 0xAD;
        dest[2] = 0xBE;
        dest[3] = 0xEF;
    }

    StaticBuffer<4> copy(buffer);
    auto view = copy.as_seq();

    REQUIRE(view[0] == 0xDE);
    REQUIRE(view[1] == 0xAD);
    REQUIRE(view[2] == 0xBE);
    REQUIRE(view[3] == 0xEF);
}

TEST_CASE(SUITE("Static buffers can be assigned"))
{
    StaticBuffer<4> buffer;
    auto dest = buffer.as_wseq();

    dest[0] = 0xDE;
    dest[1] = 0xAD;
    dest[2] = 0xBE;
    dest[3] = 0xEF;

    StaticBuffer<4> copy;
    copy = buffer;
    auto view = copy.as_seq();
    REQUIRE(view[0] == 0xDE);
    REQUIRE(view[1] == 0xAD);
    REQUIRE(view[2] == 0xBE);
    REQUIRE(view[3] == 0xEF);
}
