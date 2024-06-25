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
#ifndef SER4CPP_HEX_CONVERSIONS_H
#define SER4CPP_HEX_CONVERSIONS_H

#include "ser4cpp/container/SequenceTypes.h"
#include "ser4cpp/container/Buffer.h"

#include <memory>
#include <sstream>
#include <string>

namespace ser4cpp
{

class HexConversions
{
public:
    static char to_hex_char(char c)
    {
        return (c > 9) ? ('A' + (c - 10)) : ('0' + c);
    }

    static std::string byte_to_hex(uint8_t b)
    {
        std::ostringstream oss;
        oss << HexConversions::to_hex_char((b & 0xf0) >> 4) << HexConversions::to_hex_char(b & 0xf);
        return oss.str();
    }

    static std::string to_hex(const uint8_t* buffer, size_t length, bool spaced = false)
    {
        std::ostringstream oss;
        size_t last = length - 1;
        for (size_t i = 0; i < length; i++)
        {
            char c = buffer[i];
            oss << HexConversions::to_hex_char((c & 0xf0) >> 4) << HexConversions::to_hex_char(c & 0xf);
            if (spaced && i != last)oss << " ";
        }
        return oss.str();
    }

    static std::string to_hex(const rseq_t& buffer, bool spaced = true)
    {
        return to_hex(buffer, buffer.length(), spaced);
    }

    static std::string append_hex(std::initializer_list<std::string> segments)
    {
        std::ostringstream oss;

        for (auto& str : segments)
        {
            oss << str;
        }

        return to_hex(from_hex(oss.str())->as_rslice());
    }

    static std::string repeat_hex(uint8_t byte, uint16_t count, bool spaced = true)
    {
        Buffer buffer(count);
        buffer.as_wslice().set_all_to(byte);
        return to_hex(buffer.as_rslice(), spaced);
    }

    static std::string increment_hex(uint8_t start, uint16_t count, bool spaced = true)
    {
        Buffer buffer(count);

        for (uint16_t i = 0; i < count; ++i)
        {
            buffer.as_wslice()[i] = start;
            ++start;
        }

        return to_hex(buffer.as_rslice(), spaced);
    }

    static std::unique_ptr<Buffer> from_hex(const std::string& hex)
    {
        // create a copy of the string without space
        const std::string copy = HexConversions::remove_spaces(hex);

        //annoying when you accidentally put an 'O' instead of zero '0'
        if (copy.find_first_of("oO") != std::string::npos)
        {
            throw std::invalid_argument("Sequence contains 'o' or 'O'");
        }

        if (copy.size() % 2 != 0)
        {
            throw std::invalid_argument(hex);
        }

        const auto num_bytes = copy.size() / 2;

        auto buffer = std::make_unique<Buffer>(num_bytes);

        for (size_t index = 0, pos = 0; pos < copy.size(); ++index, pos += 2)
        {
            uint32_t val;
            std::stringstream ss;
            ss << std::hex << copy.substr(pos, 2);
            if ((ss >> val).fail())
            {
                throw std::invalid_argument(hex);
            }
            buffer->as_wslice()[index] = static_cast<uint8_t>(val);
        }

        return buffer;
    }

private:
    static void remove_spaces_in_place(std::string& hex)
    {
        size_t pos = hex.find_first_of(' ');
        if (pos != std::string::npos)
        {
            hex.replace(pos, 1, "");
            remove_spaces_in_place(hex);
        }
    }

    static std::string remove_spaces(const std::string& hex)
    {
        std::string copy(hex);
        remove_spaces_in_place(copy);
        return copy;
    }
};

}

#endif
