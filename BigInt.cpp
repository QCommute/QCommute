#include "BigInt.h"

BigInt::BigInt() {
    mpz_init(data_);
}

BigInt::BigInt(const std::string& str) {
    mpz_init(data_);
    mpz_set_str(data_, str.c_str(), 10);
}

BigInt::BigInt(const BigInt& other) {
    mpz_init(data_);
    mpz_set(data_, other.data_);
}

BigInt& BigInt::operator=(const BigInt& other) {
    if(this == &other)
        return *this;
    mpz_set(data_, other.data_);
    return *this;
}

BigInt::BigInt(BigInt&& other) {
    mpz_init(data_);
    mpz_set(data_, other.data_);
}

BigInt& BigInt::operator=(BigInt&& other) {
    if(this == &other)
        return *this;
    mpz_set(data_, other.data_);
    return *this;
}

bool BigInt::operator==(const BigInt& other) const {
    return mpz_cmp(data_, other.data_) == 0;
}

bool BigInt::operator==(int n) const {
    return mpz_cmp_si(data_, n) == 0;
}

BigInt& BigInt::operator+=(const BigInt& other) {
    mpz_add(data_, data_, other.data_);
    return *this;
}

BigInt& BigInt::operator*=(const BigInt& other) {
    mpz_mul(data_, data_, other.data_);
    return *this;
}

BigInt& BigInt::operator*=(int n) {
    mpz_mul_si(data_, data_, n);
    return *this;
}

void BigInt::print() const {
    mpz_out_str(stdout, 10, data_);
}

BigInt::~BigInt() {
    mpz_clear(data_);
}

