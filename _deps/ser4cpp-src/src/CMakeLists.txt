set(ser4cpp_public_headers
    ./ser4cpp/container/Array.h
    ./ser4cpp/container/ArrayView.h
    ./ser4cpp/container/Buffer.h
    ./ser4cpp/container/HasLength.h
    ./ser4cpp/container/Pair.h
    ./ser4cpp/container/RSeq.h
    ./ser4cpp/container/SequenceTypes.h
    ./ser4cpp/container/Settable.h
    ./ser4cpp/container/StaticBuffer.h
    ./ser4cpp/container/WSeq.h

    ./ser4cpp/serialization/BigEndian.h
    ./ser4cpp/serialization/DoubleFloat.h
    ./ser4cpp/serialization/EndianHelpers.h
    ./ser4cpp/serialization/FloatByteOrder.h
    ./ser4cpp/serialization/LittleEndian.h
    ./ser4cpp/serialization/SerializationTemplates.h
    ./ser4cpp/serialization/Serializer.h
    ./ser4cpp/serialization/SingleFloat.h
    ./ser4cpp/serialization/UInt48Type.h

    ./ser4cpp/util/Comparisons.h
    ./ser4cpp/util/HexConversions.h
    ./ser4cpp/util/Uncopyable.h
)

add_library(ser4cpp INTERFACE)
#target_sources(ser4cpp INTERFACE ${ser4cpp_public_headers})
target_compile_features(ser4cpp INTERFACE cxx_std_14)
target_include_directories(ser4cpp INTERFACE .)
