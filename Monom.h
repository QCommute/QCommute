#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <typeinfo>
#include <typeindex>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include "BigInt.h"

class Monom {
private:
    std::vector<uint8_t> pows; //список степеней
    BigInt coef; //коэффициент
public:
    Monom(const std::string &s);

    bool operator==(const Monom& other) const;
    bool isZero() const;
    int comparePowers(const Monom& other) const;
    
    void Sum(const Monom &mn);
    Monom& operator*=(const Monom& other);
    Monom& operator*=(int n);

    void print() const;
};

