#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

/**
 * @file SerializationHelpers.h
 * @brief Low-level helpers for reading and writing fixed-width integers
 *        into/from flat byte buffers.
 *
 * The serialisation format is little-endian for multi-byte values.
 * Every `serialize*` function appends to the supplied `res` vector.
 * Every `deserialize*` function reads from `data` at position `ptr` and
 * advances `ptr` by the number of bytes consumed.
 *
 * These helpers are used throughout the codebase so that BigInt, Poly, and
 * Expression objects can be saved to disk in a compact, self-describing format
 * without relying on external serialisation libraries.
 */

/** @brief Append a single byte to @p res. */
void serializeUInt8 (std::vector<uint8_t>& res, uint8_t  x);

/** @brief Append a 32-bit unsigned integer (little-endian) to @p res. */
void serializeUInt32(std::vector<uint8_t>& res, uint32_t x);

/** @brief Append a 32-bit signed integer (little-endian) to @p res. */
void serializeInt32 (std::vector<uint8_t>& res, int32_t  x);

/** @brief Read and return one byte from @p data at @p ptr; advance @p ptr by 1. */
uint8_t  deserializeUInt8 (const std::vector<uint8_t>& data, size_t& ptr);

/** @brief Read and return a 32-bit unsigned integer (little-endian); advance @p ptr by 4. */
uint32_t deserializeUInt32(const std::vector<uint8_t>& data, size_t& ptr);

/** @brief Read and return a 32-bit signed integer (little-endian); advance @p ptr by 4. */
int32_t  deserializeInt32 (const std::vector<uint8_t>& data, size_t& ptr);