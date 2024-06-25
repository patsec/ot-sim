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
#ifndef SER4CPP_SERIALIZATIONTEMPLATES_H
#define SER4CPP_SERIALIZATIONTEMPLATES_H

#include "ser4cpp/container/SequenceTypes.h"
#include "ser4cpp/serialization/UInt48Type.h"

#include <cstdint>
#include <cstring>

// On linux (at least Ubuntu 18.10), termios defines B0
// as a bitrate constant, making the templates compilation
// crash. For now, we just undef it.
#ifdef B0
#undef B0
#endif

namespace ser4cpp
{

class UInt8
{
public:
    inline static bool read_from(rseq_t& input, uint8_t& out)
    {
        if (input.length() < size) return false;

        out = read(input);
        input.advance(size);
        return true;
    }

    static bool write_to(wseq_t& dest, uint8_t value)
    {
        if (dest.length() < size) return false;

        write(dest, value);
        dest.advance(size);
        return true;
    }

    const static size_t size = 1;
    const static uint8_t min_value = 0;
    const static uint8_t max_value = 255;

    typedef uint8_t type_t;

private:
    inline static uint8_t read(const uint8_t* start)
    {
        return (*start);
    }

    inline static void write(uint8_t* start, uint8_t value)
    {
        *(start) = value;
    }
};

template <class T, uint8_t B0, uint8_t B1>
class Bit16
{
    static_assert(sizeof(T) == 2, "bad size");
    static_assert((B0 < sizeof(T))&& (B1 < sizeof(T))&& (B0 != B1), "bad config");

public:
    static bool write_to(wseq_t& dest, T value)
    {
        if (dest.length() < size) return false;

        write(dest, value);
        dest.advance(size);
        return true;
    }

    inline static bool read_from(rseq_t& input, T& out)
    {
        if (input.length() < size) return false;

        out = read(input);
        input.advance(size);
        return true;
    }

    typedef T type_t;

    const static size_t size = sizeof(T);
    const static T max_value;
    const static T min_value;

private:
    static T read(const uint8_t* data)
    {
        return (static_cast<T>(data[B0]) << 0) | (static_cast<T>(data[B1]) << 8);
    }

    static void write(uint8_t* data, T value)
    {
        data[B0] = static_cast<uint8_t>(value & 0xFF);
        data[B1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    }
};

template <class T, uint8_t B0, uint8_t B1>
const T Bit16<T, B0, B1>::max_value = std::numeric_limits<T>::max();

template <class T, uint8_t B0, uint8_t B1>
const T Bit16<T, B0, B1>::min_value = std::numeric_limits<T>::min();

template <class T, uint8_t B0, uint8_t B1, uint8_t B2, uint8_t B3>
class Bit32
{
    static_assert(sizeof(T) == 4, "bad size");
    static_assert((B0 < sizeof(T))&& (B1 < sizeof(T))&& (B2 < sizeof(T))&& (B3 < sizeof(T)), "bad config");

public:
    static bool write_to(wseq_t& dest, T value)
    {
        if (dest.length() < size) return false;

        write(dest, value);
        dest.advance(size);
        return true;
    }

    inline static bool read_from(rseq_t& input, T& out)
    {
        if (input.length() < size) return false;

        out = read(input);
        input.advance(size);
        return true;
    }

    typedef T type_t;

    const static size_t size = sizeof(T);
    const static T max_value;
    const static T min_value;

private:
    static T read(const uint8_t* data)
    {
        return	(static_cast<T>(data[B0]) << 0) |
                (static_cast<T>(data[B1]) << 8) |
                (static_cast<T>(data[B2]) << 16) |
                (static_cast<T>(data[B3]) << 24);
    }

    static void write(uint8_t* data, T value)
    {
        data[B0] = static_cast<uint8_t>(value & 0xFF);
        data[B1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        data[B2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        data[B3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    }
};

template <class T, uint8_t B0, uint8_t B1, uint8_t B2, uint8_t B3>
const T Bit32<T, B0, B1, B2, B3>::max_value = std::numeric_limits<T>::max();

template <class T, uint8_t B0, uint8_t B1, uint8_t B2, uint8_t B3>
const T Bit32<T, B0, B1, B2, B3>::min_value = std::numeric_limits<T>::min();

template <class T, uint8_t B0, uint8_t B1, uint8_t B2, uint8_t B3, uint8_t B4, uint8_t B5, uint8_t B6, uint8_t B7>
class Bit64
{
    static_assert(sizeof(T) == 8, "bad size");
    static_assert(
        (B0 < sizeof(T))&& (B1 < sizeof(T))&& (B2 < sizeof(T))&& (B3 < sizeof(T))&& (B4 < sizeof(T))&& (B5 < sizeof(T))&& (B6 < sizeof(T))&& (B7 < sizeof(T)),
        "bad config"
    );

public:
    static bool write_to(wseq_t& dest, T value)
    {
        if (dest.length() < size) return false;

        write(dest, value);
        dest.advance(size);
        return true;
    }

    inline static bool read_from(rseq_t& input, T& out)
    {
        if (input.length() < size) return false;

        out = read(input);
        input.advance(size);
        return true;
    }

    typedef T type_t;

    const static size_t size = sizeof(T);
    const static T max_value;
    const static T min_value;

private:
    static T read(const uint8_t* data)
    {
        return	(static_cast<T>(data[B0]) << 0) |
                (static_cast<T>(data[B1]) << 8) |
                (static_cast<T>(data[B2]) << 16) |
                (static_cast<T>(data[B3]) << 24) |
                (static_cast<T>(data[B4]) << 32) |
                (static_cast<T>(data[B5]) << 40) |
                (static_cast<T>(data[B6]) << 48) |
                (static_cast<T>(data[B7]) << 56);
    }

    static void write(uint8_t* data, T value)
    {
        data[B0] = static_cast<uint8_t>(value & 0xFF);
        data[B1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        data[B2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        data[B3] = static_cast<uint8_t>((value >> 24) & 0xFF);
        data[B4] = static_cast<uint8_t>((value >> 32) & 0xFF);
        data[B5] = static_cast<uint8_t>((value >> 40) & 0xFF);
        data[B6] = static_cast<uint8_t>((value >> 48) & 0xFF);
        data[B7] = static_cast<uint8_t>((value >> 56) & 0xFF);
    }
};

template <class T, uint8_t B0, uint8_t B1, uint8_t B2, uint8_t B3, uint8_t B4, uint8_t B5, uint8_t B6, uint8_t B7>
const T Bit64<T, B0, B1, B2, B3, B4, B5, B6, B7>::max_value = std::numeric_limits<T>::max();

template <class T, uint8_t B0, uint8_t B1, uint8_t B2, uint8_t B3, uint8_t B4, uint8_t B5, uint8_t B6, uint8_t B7>
const T Bit64<T, B0, B1, B2, B3, B4, B5, B6, B7>::min_value = std::numeric_limits<T>::min();


template <uint8_t B0, uint8_t B1, uint8_t B2, uint8_t B3, uint8_t B4, uint8_t B5>
class UBit48
{
    static_assert((B0 < 6)&& (B1 < 6)&& (B2 < 6)&& (B3 < 6)&& (B4 < 6)&& (B5 < 6), "bad config");

public:
    inline static bool read_from(rseq_t& input, UInt48Type& out)
    {
        if (input.length() < size) return false;

        out = read(input);
        input.advance(size);
        return true;
    }

    static bool write_to(wseq_t& dest, UInt48Type value)
    {
        if (dest.length() < size) return false;

        write(dest, value);
        dest.advance(size);
        return true;
    }

    typedef UInt48Type type_t;
    const static size_t size = 6;
    const static uint64_t min_value = 0;
    const static uint64_t max_value = 281474976710655ULL; // 2^48 -1

    static inline UInt48Type read(const uint8_t* data)
    {
        return UInt48Type(
                    (static_cast<uint64_t>(data[B0]) << 0) |
                    (static_cast<uint64_t>(data[B1]) << 8) |
                    (static_cast<uint64_t>(data[B2]) << 16) |
                    (static_cast<uint64_t>(data[B3]) << 24) |
                    (static_cast<uint64_t>(data[B4]) << 32) |
                    (static_cast<uint64_t>(data[B5]) << 40));
    }

    static inline void write(uint8_t* data, UInt48Type value)
    {
        if (value > max_value)
        {
            value = UInt48Type(max_value);
        }

        data[B0] = static_cast<uint8_t>(value & 0xFF);
        data[B1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        data[B2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        data[B3] = static_cast<uint8_t>((value >> 24) & 0xFF);
        data[B4] = static_cast<uint8_t>((value >> 32) & 0xFF);
        data[B5] = static_cast<uint8_t>((value >> 40) & 0xFF);
    }
};

}

#endif
