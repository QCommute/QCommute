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
#include <memory>

//Базовый класс для множителей выражения
class Sigma
{
protected:
    //Тип элемента (Operator, Delta, Ghost)
    //int type;
public:
    //Функция вывода объекта на экран. Зануление делает класс Base абстрактным и заставляет каждый его наследник перегружать функцию print
    virtual void print() = 0;
   // virtual bool operator==(Base) = 0;
    virtual int getType() const = 0;
    //virtual int getVal()=0;
    //virtual void setVal(int val_)=0;
    virtual size_t getHash() = 0;
    //Деструктор, в данном случае важно что он виртуальный, чтобы для наследников запускались их деструкторы, а не только деструктор базового класса
    virtual ~Sigma() = default;
};

class Sigma1d: public Sigma
{
private:
    //точка
    int p;
    //тип матрицы
    int c;
public:
    //Конструктор от простого оператора и аргумента
    Sigma1d(int p_, int c_);
    //Конструктор от строки
    bool operator==(const Sigma1d &sg);
    Sigma1d(const std::string &s, int pos, int &npos);
    //Перегрузка оператора присваивания
    Sigma1d& operator=(const Sigma1d &sg);
    //Перегрузка конструктора копирования (последние 2 важны т.к. мы храним список указателей и при копировании недостаточно просто скопировать указатели, нужно копировать память на которую они ссылаются. Конструктор копирования и оператор присваивания недостаточно умны чтобы это делать, поэтому их приходится переопределять и перегружать самостоятельно)
    Sigma1d(const Sigma1d &op);
    //Вывод
    void print();
    int getType() const override;
    int getPoint();
    int getC();
    void setC(int c_);
    void setPoint(int p_);
    size_t getHash() override;
    bool operator<(const Sigma1d& other) const;
    ~Sigma1d() override = default;
};


class Sigma2d: public Sigma
{
private:
    //точка
    int p1;
    int p2;
    //тип матрицы
    int c;
public:
    //Конструктор от простого оператора и аргумента
    Sigma2d(int p1_, int p2_, int c);
    //Конструктор от строки
    bool operator==(const Sigma2d &sg);
    Sigma2d(const std::string &s, int pos, int &npos);
    //Перегрузка оператора присваивания
    Sigma2d& operator=(const Sigma2d &sg);
    //Перегрузка конструктора копирования (последние 2 важны т.к. мы храним список указателей и при копировании недостаточно просто скопировать указатели, нужно копировать память на которую они ссылаются. Конструктор копирования и оператор присваивания недостаточно умны чтобы это делать, поэтому их приходится переопределять и перегружать самостоятельно)
    Sigma2d(const Sigma2d &op);
    //Вывод
    void print();
    int getType() const override;
    int getPoint1();
    int getPoint2();
    int getC();
    void setC(int c_);
    void setPoint1(int p1_);
    void setPoint2(int p2_);
    size_t getHash() override;
    bool operator<(const Sigma2d& other) const;
    ~Sigma2d() override = default;
};

class Sigma3d: public Sigma
{
private:
    //точка
    int p1;
    int p2;
    int p3;
    //тип матрицы
    int c;
public:
    //Конструктор от простого оператора и аргумента
    Sigma3d(int p1_, int p2_, int p3_, int c);
    bool operator==(const Sigma3d &sg);
    //Конструктор от строки
    Sigma3d(const std::string &s, int pos, int &npos);
    //Перегрузка оператора присваивания
    Sigma3d& operator=(const Sigma3d &sg);
    //Перегрузка конструктора копирования (последние 2 важны т.к. мы храним список указателей и при копировании недостаточно просто скопировать указатели, нужно копировать память на которую они ссылаются. Конструктор копирования и оператор присваивания недостаточно умны чтобы это делать, поэтому их приходится переопределять и перегружать самостоятельно)
    Sigma3d(const Sigma3d &op);
    //Вывод
    void print();
    int getType() const override;
    int getPoint1();
    void setPoint(int p1_, int p2_, int p3_);
    size_t getHash() override;
    bool operator<(const Sigma3d& other) const;
    ~Sigma3d() override = default;
};



//Важная функция глубокого копирования двух объектов типа Base. Дело в том что при копировании указателей недостаточно просто присвоить их. Вернее в каких-то ситуациях и достаточно, но тогда они будут ссылаться на одну и ту же область в памяти. И если мы изменим ее с помощью одного указателя или еще хуже - освободим память, то изменения затронут и все остальные указатели. Чтобы такого не происходило нужно копировать память. Эта функция копирует два указателя, копируя именно содержимое, т.е. память объектов типа Base, при этом понимяя тип исходного указателя (Operator, Delta, Ghost) и создавая объект того же типа
void deepCopyBase(std::shared_ptr<Sigma> &to, std::shared_ptr<Sigma> from);
