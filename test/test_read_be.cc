#include "/home/wqi/ultraHFT/common_tools/big_to_small_endian.h"
#include<iostream>
#include<cassert>
int main()
{
  
    {
        uint8_t p[] = {0x00, 0x0c};
        assert(read_be16(p) == 12);
    }

    {
        uint8_t p[] = {0x12, 0x34};
        assert(read_be16(p) == 0x1234);
    }

    {
        uint8_t p[] = {0x00, 0x00, 0x00, 0x64};
        assert(read_be32(p) == 100);
    }

    {
        uint8_t p[] = {0x12, 0x34, 0x56, 0x78};
        assert(read_be32(p) == 0x12345678);
    }

    {
        uint8_t p[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
        assert(read_be48(p) == 0x010203040506ULL);
    }

    {
        uint8_t p[] = {
            0x01, 0x23, 0x45, 0x67,
            0x89, 0xab, 0xcd, 0xef
        };

        assert(read_be64(p) == 0x0123456789abcdefULL);
    }

    uint8_t p1[] = {0x00, 0x0c};
assert(read_be16(p1) == 12);

uint8_t p2[] = {0x00, 0x27};
assert(read_be16(p2) == 39);

uint8_t p3[] = {0x00, 0x00, 0x00, 0x64};
assert(read_be32(p3) == 100);

uint8_t p4[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
assert(read_be48(p4) == 0x010203040506ULL);

    std::cout << "all tests passed\n";
  
}