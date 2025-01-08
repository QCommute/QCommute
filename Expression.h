#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <typeinfo>
#include <typeindex>
#include <cmath>
#include <cstdio>
#include <memory>
#include "Sigma.h"
#include "Poly.h"
int PrimeNumber(int n);
int LeviChivita(int i, int j);
bool prime(long int n);
class Sigma;

//Класс для выражения. Возможно основной класс в программе

class Expression {
    friend struct ExpressionHasher;
private:
    //коэффициент
    Poly poly;
    //Список сигм
    std::vector<std::shared_ptr<Sigma>> lst;

    bool im;
public:
    Expression() = default;
    //Конструктор от строки
    Expression(const std::string& s);
    //Конструктор от списка множителей и коэффициента, который по умолчанию 1
    Expression(const std::vector<std::shared_ptr<Sigma>> &v, const Poly& r, bool imm);
    //Консруктор копирования о оператор присваивания, про них уже было в Base.h
    Expression(const Expression &ex1);
    void Commute(std::vector <Expression> &O1,  const Expression &O);
    Expression& operator=(const Expression &ex1);
    bool operator==(const Expression &ex1) const;
    int getLength() const;
    uint8_t TotalLength();
    uint8_t MaxPoint();
    void MakeMbr(mpz_t mmbr_x, mpz_t mbr_y, mpz_t mbr_z, uint8_t Mp);
    std::vector<std::shared_ptr<Sigma>> getList() const;
    Poly& getPoly();
    const Poly& getPoly() const;
    void print();
    ~Expression();
};

struct ExpressionHasher {
    size_t operator()(const Expression& expr) const;
};
