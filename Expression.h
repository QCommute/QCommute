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
#include "Monom.h"
int PrimeNumber(int n);
int LeviChivita(int i, int j);
bool prime(long int n);
class Sigma;
class Monom;

//Класс для выражения. Возможно основной класс в программе

class Expression
{
private:
    //коэффициент
    std::vector<std::shared_ptr<Monom>> poly;
    //Список сигм
    std::vector<std::shared_ptr<Sigma>> lst;

    bool im;
public:
    //Конструктор от строки
    Expression(std::string s);
    //Конструктор от списка множителей и коэффициента, который по умолчанию 1
    Expression(const std::vector<std::shared_ptr<Sigma>> &v, const std::vector<std::shared_ptr<Monom>> &r, bool imm);
    //Консруктор копирования о оператор присваивания, про них уже было в Base.h
    Expression(const Expression &ex1);
    void Commute(std::vector <Expression> &O1,  const Expression &O);
    Expression& operator=(const Expression &ex1);
    bool operator==(const Expression &ex1);
    int getLength() const;
    void PolyErase(int i);
    uint8_t TotalLength();
    uint8_t MaxPoint();
    void MakeMbr(mpz_t mmbr_x, mpz_t mbr_y, mpz_t mbr_z, uint8_t Mp);
    void PolyAdd(std::shared_ptr<Monom> t);
    std::vector<std::shared_ptr<Sigma>> getList() const;
    std::vector<std::shared_ptr<Monom>> getPoly();
    void setCoeff(std::vector<std::shared_ptr<Monom>> &r);
    void print();
    ~Expression();
};
