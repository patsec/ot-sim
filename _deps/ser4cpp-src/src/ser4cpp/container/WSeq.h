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
#ifndef SER4CPP_WSEQ_H
#define SER4CPP_WSEQ_H

#include "ser4cpp/container/RSeq.h"
#include "ser4cpp/util/Comparisons.h"

#include <cstdint>
#include <cstring>

namespace ser4cpp
{

/**
*	Represents a write-able slice of a buffer located elsewhere. Mediates writing to the buffer
*	to prevent overruns and other errors. Parameterized by the length type
*/
template <class L>
class WSeq : public HasLength<L>
{
    typedef void* (memfunc_t)(void*, const void*, size_t);

public:
    static WSeq empty()
    {
        return WSeq();
    }

    WSeq()
    {}

    WSeq(uint8_t* buffer, L length) : HasLength<L>(length), buffer_(buffer)
    {}

    void set_all_to(uint8_t value)
    {
        memset(this->buffer_, value, this->length());
    }

    void make_empty()
    {
        this->buffer_ = nullptr;
        this->m_length = 0;
    }

    L advance(L count)
    {
        const auto num = ser4cpp::min(count, this->length());
        this->buffer_ += num;
        this->m_length -= num;
        return num;
    }

    bool put(uint8_t byte)
    {
        if (this->length() == 0) return false;
        else
        {
            this->buffer_[0] = byte;
            ++this->buffer_;
            --this->m_length;
            return true;
        }
    }

    WSeq skip(size_t count) const
    {
        const auto num = ser4cpp::min(count, this->m_length);
        return WSeq(this->buffer_ + num, this->m_length - num);
    }

    WSeq take(size_t count) const
    {
        return WSeq(this->buffer_, ser4cpp::min(this->m_length, count));
    }

    RSeq<L> readonly() const
    {
        return RSeq<L>(this->buffer_, this->length());
    }

    operator uint8_t* () const
    {
        return buffer_;
    };

    RSeq<L> copy_from(const RSeq<L>& src)
    {
        return this->transfer_from<memcpy>(src);
    }

    RSeq<L> move_from(const RSeq<L>& src)
    {
        return this->transfer_from<memmove>(src);
    }

private:
    template <memfunc_t mem_func>
    RSeq<L> transfer_from(const RSeq<L>& src)
    {
        if (src.length() > this->length())
        {
            return RSeq<L>::empty();
        }
        else
        {
            const auto ret = this->readonly().take(src.length());
            mem_func(buffer_, src, src.length());
            this->advance(src.length());
            return ret;
        }
    }

    uint8_t* buffer_ = nullptr;
};

}

#endif
