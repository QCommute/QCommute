#pragma once

#include <vector>
#include <string>
#include <functional>
#include "BigInt.h"

// Polynomial with arbitrary-precision (BigInt) coefficients. Maximum supported degree per variable = 253.
// The number of variables is unlimited.
class Poly {
    friend class Poly2;
private:
    // invariants:
    // * terms are sorted in lexicographic order of their degree vectors;
    // * no like terms;
    // * no terms with a zero coefficient
    //std::vector<Monom> data_;
    std::vector<uint8_t> powers_;
    std::vector<BigInt> coefs_;

    static constexpr uint8_t MONOM_END = 254;
    static constexpr uint8_t POLY_END = 255;

    Poly(std::vector<uint8_t>&& powers_, const std::vector<BigInt>& coefs);
public:
    Poly();
    Poly(const std::string& str);
    Poly(const std::vector<uint8_t>& data, size_t& ptr);

    bool isZero() const;
    void print() const;
    void serialize(std::vector<uint8_t>& res) const;

    Poly prependVariables(size_t numVariables) const;

    Poly& operator+=(const Poly& other);
    Poly operator*(const Poly& other) const;
    // Exact equality, including coefficients
    bool operator==(const Poly& other) const;
    Poly& operator*=(int n);
    Poly operator*(int n) const;
};