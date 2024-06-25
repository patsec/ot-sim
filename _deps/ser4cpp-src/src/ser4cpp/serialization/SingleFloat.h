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
#ifndef SER4CPP_SINGLE_FLOAT_H
#define SER4CPP_SINGLE_FLOAT_H

#include "ser4cpp/util/Uncopyable.h"
#include "ser4cpp/container/SequenceTypes.h"

#include <cstddef>
#include <limits>
#include <cmath>

namespace ser4cpp
{

class SingleFloat : private StaticOnly
{
public:
    static_assert(sizeof(float) == 4, "Unexpected length of single float");

    static bool read_from(rseq_t& input, float& out)
    {
        uint32_t value;
        if(UInt32::read_from(input, value))
        {
            out = to_float32(value);
            return true;
        }
        
        return false;
    }

    static bool write_to(wseq_t& dest, float value)
    {
        if (dest.length() < size) return false;

        const auto uint32_value = to_uint32(value);
        return UInt32::write_to(dest, uint32_value);
    }

    static float to_float32(const uint32_t value)
    {
        // Extract the required values
        bool sign = (value & 0x80000000) != 0;
        uint8_t exponent = static_cast<uint8_t>((value >> 23) & 0xFF);
        uint32_t mantissa = value & 0x7FFFFF;

        // Check special cases
        if(exponent == 0xFF)
        {
            if(mantissa != 0)
            {
                return std::numeric_limits<float>::quiet_NaN();
            }
            else
            {
                if(!sign)
                {
                    return std::numeric_limits<float>::infinity();
                }
                else
                {
                    return -std::numeric_limits<float>::infinity();
                }
                
            }
        }
        if(exponent == 0 && mantissa == 0)
        {
            return 0.0f;
        }

        // Build the actual value
        auto weighted_mantissa = static_cast<float>(mantissa)/(uint32_t{1} << 23);
        float result;
        if(exponent == 0)
        {
            result = std::ldexp(weighted_mantissa, 2 - (uint16_t{1} << 8));
        }
        else
        {
            result = std::ldexp(1.0f + weighted_mantissa, exponent - 127);
        }

        // Adjust the sign
        if(sign)
        {
            result = -result;
        }

        return result;
    }

    static uint32_t to_uint32(const float value)
    {
        uint32_t encoded_value{0};

        if (std::isnan(value))
        {
            // NaN has all exponent bit set to 1, and mantissa with a least a 1. Sign bit is ignored.
            // Here, I use x86 qNaN (because libiec61850 simply cast the value into a double)
            encoded_value = 0x7F800001;
        }
        else if (std::isinf(value))
        {
            // Infinite has all exponent bit set to 1, and mantissa filled with zeroes. Sign bit determines
            // which infinite it represents
            encoded_value = 0x7F800000;
        }
        else if (value != 0.0f)
        {
            int integral_part;
            float fraction_part = std::frexp(std::abs(value), &integral_part);

            uint16_t exponent = integral_part + 126;
            encoded_value |= (static_cast<uint32_t>(exponent) & 0xFF) << 23;
            encoded_value |= static_cast<uint32_t>(fraction_part * (uint32_t{1} << 24)) & 0x007FFFFF;
        }

        if (std::signbit(value))
        {
            encoded_value |= uint32_t{1} << 31;
        }

        return encoded_value;
    }

    typedef float type_t;
    static constexpr std::size_t size = sizeof(float);
    static constexpr float max_value = std::numeric_limits<float>::max();
    static constexpr float min_value = -std::numeric_limits<float>::max();
};

}

#endif
