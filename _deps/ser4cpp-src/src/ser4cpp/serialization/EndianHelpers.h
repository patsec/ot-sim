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
#ifndef SER4CPP_ENDIANHELPERS_H
#define SER4CPP_ENDIANHELPERS_H

#include "ser4cpp/util/Uncopyable.h"

namespace ser4cpp
{

namespace serializers
{

template<typename T>
bool read_one(rseq_t& input, T& out);

template<>
inline bool read_one(rseq_t& input, uint8_t& out)
{
    return UInt8::read_from(input, out);
}

template<>
inline bool read_one(rseq_t& input, int16_t& out)
{
    return Int16::read_from(input, out);
}

template<>
inline bool read_one(rseq_t& input, uint16_t& out)
{
    return UInt16::read_from(input, out);
}

template<>
inline bool read_one(rseq_t& input, int32_t& out)
{
    return Int32::read_from(input, out);
}

template<>
inline bool read_one(rseq_t& input, uint32_t& out)
{
    return UInt32::read_from(input, out);
}

template<>
inline bool read_one(rseq_t& input, int64_t& out)
{
    return Int64::read_from(input, out);
}

template<>
inline bool read_one(rseq_t& input, uint64_t& out)
{
    return UInt64::read_from(input, out);
}

template<>
inline bool read_one(rseq_t& input, UInt48Type& out)
{
    return UInt48::read_from(input, out);
}

template<>
inline bool read_one(rseq_t& input, double& out)
{
    return DoubleFloat::read_from(input, out);
}

template<>
inline bool read_one(rseq_t& input, float& out)
{
    return SingleFloat::read_from(input, out);
}

template<typename T>
bool write_one(wseq_t& dest, const T& value);

template<>
inline bool write_one(wseq_t& dest, const uint8_t& value)
{
    return UInt8::write_to(dest, value);
}

template<>
inline bool write_one(wseq_t& dest, const int16_t& value)
{
    return Int16::write_to(dest, value);
}

template<>
inline bool write_one(wseq_t& dest, const uint16_t& value)
{
    return UInt16::write_to(dest, value);
}

template<>
inline bool write_one(wseq_t& dest, const int32_t& value)
{
    return Int32::write_to(dest, value);
}

template<>
inline bool write_one(wseq_t& dest, const uint32_t& value)
{
    return UInt32::write_to(dest, value);
}

template<>
inline bool write_one(wseq_t& dest, const int64_t& value)
{
    return Int64::write_to(dest, value);
}

template<>
inline bool write_one(wseq_t& dest, const uint64_t& value)
{
    return UInt64::write_to(dest, value);
}

template<>
inline bool write_one(wseq_t& dest, const UInt48Type& value)
{
    return UInt48::write_to(dest, value);
}

template<>
inline bool write_one(wseq_t& dest, const double& value)
{
    return DoubleFloat::write_to(dest, value);
}

template<>
inline bool write_one(wseq_t& dest, const float& value)
{
    return SingleFloat::write_to(dest, value);
}

} // namespace serializers

class EndianHelpers : private StaticOnly
{

public:
    template <class T, typename... Args>
    static bool read(rseq_t& input, T& value, Args& ... args)
    {
        return serializers::read_one(input, value) && read(input, args...);
    }

    template <class T, typename... Args>
    static bool write(wseq_t& dest, const T& value, const Args& ... args)
    {
        return serializers::write_one(dest, value) && write(dest, args...);
    }

private:
    static inline bool read(rseq_t& input)
    {
        return true;
    }

    static inline bool write(wseq_t& dest)
    {
        return true;
    }
};

} // namespace ser4cpp

#endif
