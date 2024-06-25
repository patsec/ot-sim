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
#ifndef SER4CPP_DOUBLE_FLOAT_H
#define SER4CPP_DOUBLE_FLOAT_H

#include "ser4cpp/util/Uncopyable.h"
#include "ser4cpp/container/SequenceTypes.h"

#include <cstddef>
#include <limits>
#include <cmath>

namespace ser4cpp
{

class DoubleFloat : private StaticOnly
{
public:
    static_assert(sizeof(double) == 8, "Unexpected length of double float");

    static bool read_from(rseq_t& input, double& out)
    {
        uint64_t value;
        if(UInt64::read_from(input, value))
        {
            out = to_double(value);
            return true;
        }
        
        return false;
    }
    
    static bool write_to(wseq_t& dest, double value)
    {
        if (dest.length() < size) return false;

        const auto uint64_value = to_uint64(value);
        return UInt64::write_to(dest, uint64_value);
    }

    static double to_double(const uint64_t value)
    {
        // Extract the required values
        bool sign = (value & 0x8000000000000000) != 0;
        uint16_t exponent = static_cast<uint16_t>((value >> 52) & 0x7FF);
        uint64_t mantissa = value & 0xFFFFFFFFFFFFF;

        // Check special cases
        if(exponent == 0x7FF)
        {
            if(mantissa != 0)
            {
                return std::numeric_limits<double>::quiet_NaN();
            }
            else
            {
                if(!sign)
                {
                    return std::numeric_limits<double>::infinity();
                }
                else
                {
                    return -std::numeric_limits<double>::infinity();
                }
                
            }
        }
        if(exponent == 0 && mantissa == 0)
        {
            return 0.0;
        }

        // Build the actual value
        auto weighted_mantissa = static_cast<double>(mantissa)/(uint64_t{1} << 52);
        double result;
        if(exponent == 0)
        {
            result = std::ldexp(weighted_mantissa, 2 - (uint16_t{1} << 11));
        }
        else
        {
            result = std::ldexp(1.0 + weighted_mantissa, exponent - 1023);
        }

        // Adjust the sign
        if(sign)
        {
            result = -result;
        }

        return result;
    }

    static uint64_t to_uint64(const double value)
    {
        uint64_t encoded_value{0};

        if (std::isnan(value))
        {
            // NaN has all exponent bit set to 1, and mantissa with a least a 1. Sign bit is ignored.
            // Here, I use x86 qNaN (because libiec61850 simply cast the value into a double)
            encoded_value = 0x7FF8000000000001;
        }
        else if (std::isinf(value))
        {
            // Infinite has all exponent bit set to 1, and mantissa filled with zeroes. Sign bit determines
            // which infinite it represents
            encoded_value = 0x7FF0000000000000;
        }
        else if (value != 0.0)
        {
            int integral_part;
            double fraction_part = std::frexp(std::abs(value), &integral_part);

            uint16_t exponent = integral_part + 1022;
            encoded_value |= (static_cast<uint64_t>(exponent) & 0x7FF) << 52;
            encoded_value |= static_cast<uint64_t>(fraction_part * (uint64_t{1} << 53)) & 0xFFFFFFFFFFFFF;
        }

        if (std::signbit(value))
        {
            encoded_value |= uint64_t{1} << 63;
        }

        return encoded_value;
    }

    typedef double type_t;
    static constexpr std::size_t size = sizeof(double);
    static constexpr double max_value = std::numeric_limits<double>::max();
    static constexpr double min_value = -std::numeric_limits<double>::max();
};

}

#endif
