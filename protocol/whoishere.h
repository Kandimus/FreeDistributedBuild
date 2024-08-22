
#pragma once

#include <stdint.h>

namespace NetPacket
{

struct WhoIsHere
{
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t masterPort = 0;
    uint32_t masterIP = 0;
    char     project[64] = {0};
    uint32_t crc32 = 0;
};

}
