#pragma once
#include <gmpxx.h>

class BigInt {
private:
    mpz_t data_;
public:
    BigInt();
    BigInt(const std::string& str);
    
    BigInt(const BigInt& other);
    BigInt& operator=(const BigInt& other);
    BigInt(BigInt&& other);
    BigInt& operator=(BigInt&& other);

    bool operator==(const BigInt& other) const;
    bool operator==(int n) const;

    BigInt& operator+=(const BigInt& other);
    BigInt& operator*=(const BigInt& other);
    BigInt& operator*=(int n);

    void print() const;
    
    ~BigInt();
};