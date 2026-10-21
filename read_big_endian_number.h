#include<cstdint>

#pragma once
namespace read_be
{

uint16_t read_be16(const uint8_t *p)
{
    return  ((static_cast<uint16_t>(p[0])) << 8) | (static_cast<uint16_t>(p[1]));
}

uint32_t read_be32(const uint8_t *p)
{
    return (static_cast<uint32_t>(p[0]) << 24) | 
           (static_cast<uint32_t>(p[1]) << 16) | 
           (static_cast<uint32_t>(p[2]) << 8 ) | 
            static_cast<uint32_t>(p[3])        ;
}

uint64_t read_be64(const uint8_t *p)
{
    return  (static_cast<uint64_t>(p[0]) << 56) |
            (static_cast<uint64_t>(p[1]) << 48) |
            (static_cast<uint64_t>(p[2]) << 40) |
            (static_cast<uint64_t>(p[3]) << 32) |
            (static_cast<uint64_t>(p[4]) << 24) |
            (static_cast<uint64_t>(p[5]) << 16) |
            (static_cast<uint64_t>(p[6]) << 8)  |
            (static_cast<uint64_t>(p[7]));
}

// for parsing timestamp
uint64_t read_be48(const uint8_t* p)
{
    return  
        (static_cast<uint64_t>(p[0]) << 40) |
        (static_cast<uint64_t>(p[1]) << 32) |
        (static_cast<uint64_t>(p[2]) << 24) |
        (static_cast<uint64_t>(p[3]) << 16) |
        (static_cast<uint64_t>(p[4]) << 8)  |
        (static_cast<uint64_t>(p[5]));
}


}
