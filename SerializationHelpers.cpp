#include "SerializationHelpers.h"

void serializeUInt8(std::vector<uint8_t>& res, uint8_t x) {
    res.push_back(x);
}

void serializeUInt32(std::vector<uint8_t>& res, uint32_t x) {
    res.push_back(x >> 24);
    res.push_back((x >> 16) & 255);
    res.push_back((x >> 8) & 255);
    res.push_back(x & 255);
}

void serializeInt32(std::vector<uint8_t>& res, int32_t x) {
    res.push_back((*(uint32_t*)(&x)) >> 24);
    res.push_back((x >> 16) & 255);
    res.push_back((x >> 8) & 255);
    res.push_back(x & 255);
}

uint8_t deserializeUInt8(const std::vector<uint8_t>& data, size_t& ptr) {
    return data[ptr++];
}

uint32_t deserializeUInt32(const std::vector<uint8_t>& data, size_t& ptr) {
    uint32_t res = 0;
    for(int32_t i = 0; i < 4; i++)
        res = res * 256 + data[ptr + i];
    ptr += 4;
    return res;
}

int32_t deserializeInt32(const std::vector<uint8_t>& data, size_t& ptr) {
    uint32_t res = 0;
    for(int32_t i = 0; i < 4; i++)
        res = res * 256 + data[ptr + i];
    ptr += 4;
    return *((int32_t*)(&res));
}
