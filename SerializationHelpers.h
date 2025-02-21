#pragma once

#include <vector>
#include <cstdint>

void serializeUInt8(std::vector<uint8_t>& res, uint8_t x);
void serializeUInt32(std::vector<uint8_t>& res, uint32_t x);
void serializeInt32(std::vector<uint8_t>& res, int32_t x);

uint8_t deserializeUInt8(const std::vector<uint8_t>& data, size_t& ptr);
uint32_t deserializeUInt32(const std::vector<uint8_t>& data, size_t& ptr);
int32_t deserializeInt32(const std::vector<uint8_t>& data, size_t& ptr);