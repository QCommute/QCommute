#pragma once

#include <vector>
#include <memory>
#include "Monom.h"

class Poly {
private:
    std::vector<std::shared_ptr<Monom>> data_;
public:
    Poly() = default;
    Poly(const std::string& str);

    bool isZero() const;
    void print() const;

    void deepCopyFrom(const Poly& other);

    Poly& operator+=(const Poly& other);
    Poly operator*(const Poly& other) const;
    // Точное равенство, включая коэффициенты
    bool operator==(const Poly& other) const;
    Poly& operator*=(int n);
};