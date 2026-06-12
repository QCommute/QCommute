#pragma once

#include <gmpxx.h>
#include <vector>
#include <cstdint>
#include <string>

/**
 * @brief Arbitrary-precision integer backed by GNU MP (libgmp).
 *
 * Wraps `mpz_t` and exposes the minimal arithmetic interface required by
 * the Poly class: addition, multiplication, and equality testing.
 * Serialisation is done into/from a flat byte buffer so that instances can be
 * stored on disk without format conversion.
 *
 * All operations are exact (no rounding).
 */
class BigInt {
private:
    mpz_t data_;
public:
    /** @brief Default-construct as zero. */
    BigInt();

    /** @brief Construct from a decimal string, e.g. "42" or "-7". */
    BigInt(const std::string& str);

    /**
     * @brief Deserialise from a byte buffer.
     * @param data  Byte buffer produced by a previous call to serialize().
     * @param ptr   Read cursor; advanced past the consumed bytes on return.
     */
    BigInt(const std::vector<uint8_t>& data, size_t& ptr);

    BigInt(const BigInt& other);
    BigInt& operator=(const BigInt& other);
    BigInt(BigInt&& other) __GMP_NOTHROW;
    BigInt& operator=(BigInt&& other);

    bool operator==(const BigInt& other) const;
    bool operator==(int n) const;

    BigInt& operator+=(const BigInt& other);
    BigInt& operator*=(const BigInt& other);
    BigInt& operator*=(int n);

    /** @brief Print the decimal representation to stdout. */
    void print() const;

    /**
     * @brief Append the serialised representation to @p res.
     *
     * The format is self-delimiting: a deserialising BigInt constructor reads
     * exactly as many bytes as were written here.
     */
    void serialize(std::vector<uint8_t>& res) const;

    ~BigInt();
};