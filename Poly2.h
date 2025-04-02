#pragma once

#include <vector>
#include <map>
#include <functional>
#include "BigInt.h"

class Poly;

// Многчолен с коэффициентами - длинными числами. Максимальная поддерживаемая степень переменной = 253.
// Число перменных не ограничено.
class Poly2 {
private:
    // инварианты:
    // * нет слагаемых с нулевым коэффициентом
    std::map<std::vector<uint8_t>, BigInt> data_;
public:
    Poly2() = default;
    explicit Poly2(const Poly& poly);

    void printAsMathematica(size_t groupByFirst, std::function<std::string(size_t)> varNameFunc) const;

    Poly2& operator+=(const Poly2& other);
};