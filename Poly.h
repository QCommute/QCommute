#pragma once

#include <vector>
#include <string>
#include <functional>
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

    Poly(std::vector<uint8_t>&& powers_, const std::vector<BigInt>& coefs);
public:
    Poly();
    Poly(const std::string& str);
    Poly(const std::vector<uint8_t>& data, size_t& ptr);

    bool isZero() const;
    void print() const;
    void printAsMathematica(size_t groupByFirst, std::function<std::string(size_t)> varNameFunc) const;
    void serialize(std::vector<uint8_t>& res) const;

    Poly prependVariables(size_t numVariables) const;

    Poly& operator+=(const Poly& other);
    Poly operator*(const Poly& other) const;
    // Точное равенство, включая коэффициенты
    bool operator==(const Poly& other) const;
    Poly& operator*=(int n);
    Poly operator*(int n) const;
};