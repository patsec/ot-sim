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

#include "ser4cpp/container/Buffer.h"
#include "ser4cpp/serialization/LittleEndian.h"
#include "ser4cpp/serialization/FloatByteOrder.h"
#include "ser4cpp/util/HexConversions.h"

using namespace ser4cpp;
using namespace std;

template <class T>
void TestFloatParsing(std::string expected_hex, typename T::type_t value, bool check_nan = false)
{
    // Serialization test
    Buffer buffer(T::size);
    auto dest = buffer.as_wslice();

    CHECK(T::write_to(dest, value));
    CHECK(dest.is_empty());

    const auto hex = HexConversions::to_hex(buffer.as_rslice());
    CHECK(hex == expected_hex);

    // Deserialization test
    typename T::type_t read_value;

    auto expected_bytes = HexConversions::from_hex(expected_hex);
    auto input = expected_bytes->as_rslice();

    CHECK(T::read_from(input, read_value));
    CHECK(input.is_empty());

    if(!check_nan)
    {
        CHECK(value == read_value);
    }
    else
    {
        // Two NaN values are NEVER equal, even if they have the same representation
        CHECK(std::isnan(read_value));
    }
}

#define SUITE(name) "FloatSerializationTestSuite - " name

TEST_CASE(SUITE("Float memory byte order is IEEE 754"))
{
    REQUIRE(ser4cpp::FloatByteOrder::order() != FloatByteOrder::Value::unsupported);
}

TEST_CASE(SUITE("DoubleFloatSerialization"))
{
    TestFloatParsing<ser4cpp::DoubleFloat>("00 00 00 00 00 00 00 00", 0.0);
    TestFloatParsing<ser4cpp::DoubleFloat>("0A 52 84 2F C7 2B A2 C0", -2.3258890344E3);
    TestFloatParsing<ser4cpp::DoubleFloat>("00 00 00 00 64 89 67 41", 12340000.0);
    TestFloatParsing<ser4cpp::DoubleFloat>("00 00 00 00 00 00 34 C0", -20.0);
    TestFloatParsing<ser4cpp::DoubleFloat>("8F 81 9C 95 2D F9 64 BB", -13.879E-23);
    TestFloatParsing<ser4cpp::DoubleFloat>("00 00 00 00 00 00 59 40", 100.0);

    // These tests were copied from goose-cpp, but with the bytes reversed (because GOOSE is big endian)
    TestFloatParsing<ser4cpp::DoubleFloat>("9A 99 99 99 99 19 45 40", 42.2);
    TestFloatParsing<ser4cpp::DoubleFloat>("66 66 66 66 66 26 53 C0", -76.6);
    TestFloatParsing<ser4cpp::DoubleFloat>("F3 8E 53 74 24 97 BF 3F", 0.1234);
    TestFloatParsing<ser4cpp::DoubleFloat>("01 00 00 00 00 00 F8 7F", std::numeric_limits<double>::quiet_NaN(), true);
    TestFloatParsing<ser4cpp::DoubleFloat>("00 00 00 00 00 00 F0 7F", std::numeric_limits<double>::infinity());
    TestFloatParsing<ser4cpp::DoubleFloat>("00 00 00 00 00 00 F0 FF", -std::numeric_limits<double>::infinity());
}

TEST_CASE(SUITE("SingleFloatSerialization"))
{
    TestFloatParsing<ser4cpp::SingleFloat>("00 00 00 00", 0.0f);
    TestFloatParsing<ser4cpp::SingleFloat>("20 4B 3C 4B", 12340000.0f);
    TestFloatParsing<ser4cpp::SingleFloat>("6D C9 27 9B", -13.879E-23f);
    TestFloatParsing<ser4cpp::SingleFloat>("00 00 A0 C1", -20.0f);

    // These tests were copied from goose-cpp, but with the bytes reversed (because GOOSE is big endian)
    TestFloatParsing<ser4cpp::SingleFloat>("CD CC 28 42", 42.2f);
    TestFloatParsing<ser4cpp::SingleFloat>("33 33 99 C2", -76.6f);
    TestFloatParsing<ser4cpp::SingleFloat>("24 B9 FC 3D", 0.1234f);
    TestFloatParsing<ser4cpp::SingleFloat>("01 00 80 7F", std::numeric_limits<float>::quiet_NaN(), true);
    TestFloatParsing<ser4cpp::SingleFloat>("00 00 80 7F", std::numeric_limits<float>::infinity());
    TestFloatParsing<ser4cpp::SingleFloat>("00 00 80 FF", -std::numeric_limits<float>::infinity());
}
