#include "BigInt.h"
#include "SerializationHelpers.h"

BigInt::BigInt() {
    mpz_init(data_);
}

BigInt::BigInt(const std::string& str) {
    mpz_init(data_);
    mpz_set_str(data_, str.c_str(), 10);
}

BigInt::BigInt(const std::vector<uint8_t>& data, size_t& ptr) {
    mpz_init(data_);
    
    uint32_t numBytes = deserializeUInt32(data, ptr);
    bool neg = deserializeUInt8(data, ptr) > 0;
    mpz_import(data_, numBytes, 1, 1, 0, 0, data.data() + ptr);
    if(neg)
        mpz_neg(data_, data_);
    ptr += numBytes;
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

BigInt::BigInt(BigInt&& other) __GMP_NOTHROW {
    mpz_init(data_);
    mpz_swap(data_, other.data_);
}

BigInt& BigInt::operator=(BigInt&& other) {
    if(this == &other)
        return *this;
    mpz_swap(data_, other.data_);
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

void BigInt::serialize(std::vector<uint8_t>& res) const {
    size_t numBits = mpz_sizeinbase(data_, 2);
    char* bytes = new char[(numBits + 7) / 8];
    mpz_export(bytes, nullptr, 1, 1, 0, 0, data_);

    serializeUInt32(res, (numBits + 7) / 8);
    serializeUInt8(res, mpz_cmp_si(data_, 0) < 0);
    for(size_t i = 0; i < (numBits + 7) / 8; i++)
        serializeUInt8(res, *((uint8_t*)(bytes + i)));
    delete[] bytes;
}

BigInt::~BigInt() {
    mpz_clear(data_);
}

