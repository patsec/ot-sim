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
#ifndef SER4CPP_LITTLEENDIAN_H
#define SER4CPP_LITTLEENDIAN_H

#include "ser4cpp/serialization/SerializationTemplates.h"

namespace ser4cpp
{

using Int16  = Bit16<int16_t, 0, 1>;
using UInt16 = Bit16<uint16_t, 0, 1>;
using Int32  = Bit32<int32_t, 0, 1, 2, 3>;
using UInt32 = Bit32<uint32_t, 0, 1, 2, 3>;
using Int64  = Bit64<int64_t, 0, 1, 2, 3, 4, 5, 6, 7>;
using UInt64 = Bit64<uint64_t, 0, 1, 2, 3, 4, 5, 6, 7>;

using UInt48 = UBit48<0, 1, 2, 3, 4, 5>;

}

#include "ser4cpp/serialization/DoubleFloat.h"
#include "ser4cpp/serialization/SingleFloat.h"
#include "ser4cpp/serialization/EndianHelpers.h"

namespace ser4cpp
{
    using LittleEndian = EndianHelpers;
}

#endif
