#pragma once

#include <vector>
#include <memory>
#include "Monom.h"

class Poly {
private:
    // инварианты:
    // * слагаемые идут в лексикографическом порядке векторов степеней;
    // * нет подобных слагаемых;
    // * нет слагаемых с нулевым коэффициентом
    std::vector<Monom> data_;
public:
    Poly() = default;
    Poly(const std::string& str);

    bool isZero() const;
    void print() const;

    Poly& operator+=(const Poly& other);
    Poly operator*(const Poly& other) const;
    // Точное равенство, включая коэффициенты
    bool operator==(const Poly& other) const;
    Poly& operator*=(int n);
    Poly operator*(int n) const;
};