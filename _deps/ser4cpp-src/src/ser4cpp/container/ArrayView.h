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
#ifndef SER4CPP_ARRAYVIEW_H
#define SER4CPP_ARRAYVIEW_H

#include "ser4cpp/container/HasLength.h"

#include <cassert>
#include <functional>

namespace ser4cpp
{

/**
 * Acts as a safe facade around an underlying array
 */
template<class T, class W> class ArrayView : public HasLength<W>
{

public:
    static ArrayView<T, W> Empty()
    {
        return ArrayView(nullptr, 0);
    }

    ArrayView(T* start, W size) : HasLength<W>(size), m_buffer(start) {}

    inline bool contains(W index) const
    {
        return index < this->m_length;
    }

    inline bool contains(W start, W stop) const
    {
        return (start < stop) && Contains(stop);
    }

    inline T& operator[](W index)
    {
        assert(index < this->m_length);
        return m_buffer[index];
    }

    inline const T& operator[](W index) const
    {
        assert(index < this->m_length);
        return m_buffer[index];
    }

    template<class Action> void foreach(const Action& action)
    {
        for (W i = 0; i < this->m_length; ++i)
        {
            action(m_buffer[i]);
        }
    }


private:
    T* m_buffer;
};

} // namespace ser4cpp

#endif
