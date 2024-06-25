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
#ifndef SER4CPP_STATICBUFFER_H
#define SER4CPP_STATICBUFFER_H

#include "ser4cpp/util/Comparisons.h"
#include "ser4cpp/util/Uncopyable.h"
#include "ser4cpp/container/SequenceTypes.h"

#include <limits>
#include <cstdint>

namespace ser4cpp
{
template <size_t LENGTH>
class StaticBuffer final
{

public:
	StaticBuffer() = default;

    inline rseq_t as_seq() const
    {
        return rseq_t(this->buffer, LENGTH);
    }

    inline rseq_t as_seq(size_t max_size) const
    {
        return this->as_seq().take(max_size);
    }

    inline wseq_t as_wseq()
    {
        return wseq_t(this->buffer, LENGTH);
    }

    inline wseq_t as_wseq(size_t max_size)
    {
        return wseq_t(this->buffer, ser4cpp::min(LENGTH, max_size));
    }

    inline size_t length() const
    {
        return LENGTH;
    }

private:
    uint8_t buffer[LENGTH] = { 0 };
};

}

#endif
