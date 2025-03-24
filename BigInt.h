#pragma once

#include <gmpxx.h>
#include <vector>
#include <cstdint>
#include <string>

class BigInt {
private:
    mpz_t data_;
public:
    BigInt();
    BigInt(const std::string& str);
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

    void print() const;
    void serialize(std::vector<uint8_t>& res) const;
    
    ~BigInt();
};