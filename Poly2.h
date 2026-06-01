#pragma once

#include <vector>
#include <map>
#include <functional>
#include "BigInt.h"

class Poly;

// Polynomial with arbitrary-precision (BigInt) coefficients. Maximum supported degree per variable = 253.
// The number of variables is unlimited.
class Poly2 {
private:
    // invariants:
    // * no terms with a zero coefficient
    std::map<std::vector<uint8_t>, BigInt> data_;
public:
    Poly2() = default;
    explicit Poly2(const Poly& poly);

    void printAsMathematica(size_t groupByFirst, std::function<std::string(size_t)> varNameFunc) const;

    Poly2& operator+=(const Poly2& other);
};