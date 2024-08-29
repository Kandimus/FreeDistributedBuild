
#pragma once

#include <stdint.h>
#include <string>

namespace Global
{

const uint8_t versionMajor = 1;
const uint8_t versionMinor = 3;

const uint16_t tcpDefaultPort = 1290;
const uint16_t udpDefaultPort = 1291;

const std::string udpMulticastIp = "239.172.22.165";

const uint32_t tcpMagicNumber = 0xffdd0011;
const uint32_t udpMagicNumber = 0xaaa33388;

const std::string fileProjects = "FreeDistributedBuild.xml";

}
