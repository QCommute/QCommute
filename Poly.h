#pragma once

#include <vector>
#include <memory>
#include "BigInt.h"

// Многчолен с коэффициентами - длинными числами. Максимальная поддерживаемая степень переменной = 253.
// Число перменных не ограничено.
class Poly {
private:
    // инварианты:
    // * слагаемые идут в лексикографическом порядке векторов степеней;
    // * нет подобных слагаемых;
    // * нет слагаемых с нулевым коэффициентом
    //std::vector<Monom> data_;
    std::vector<uint8_t> powers_;
    std::vector<BigInt> coefs_;

    static constexpr uint8_t MONOM_END = 254;
    static constexpr uint8_t POLY_END = 255;
public:
    Poly();
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