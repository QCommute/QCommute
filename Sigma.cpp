#include "Sigma.h"
#include <cstdlib>
#include <memory>

extern int indcount;
extern int lorindcount;

 //описание класса Sigma тут особо комментировать нечего, разве что тут фактически три класса для одномерной, двумерной или трехмерной задачи
int Sigma1d::getPoint() const {
    return p;
}
int Sigma1d::getC() const {
    return c;
}
void Sigma1d::setPoint(int p_)
{
    p=p_;
}
void Sigma1d::setC(int c_)
{
    c=c_;
}
Sigma1d::Sigma1d(int p_, int c_)
{
    p=p_;
    c=c_;
}
Sigma1d::Sigma1d(const std::string &s, int pos, int &npos)
{
    p=1;
    c=1;
    //Проверяет что строка начинается действительно с D или g и в случае успеха распознаем эти операторы (до аргументов), выделяем под них память и кладем в список
    if (s[pos] != 'S') {
        assert(0);
    }
    pos++;
    if (s[pos] == 'x') {
        c=1;
    }
        else{
            if (s[pos] == 'y') {
                c=2;
            }
            else{
                 if (s[pos] == 'z') {
                     c=3;
                 }
                 else{
                    assert(0);
                 }
            }
        }

    pos++;
    if (s[pos] != '(') {
        assert(0);
    }
    pos++;
    if (s[pos] >= '0' && s[pos] <= '9'){
        p = s[pos] - '0';
    }
    else{
        assert(0);
    }
    pos++;
    if (s[pos] == ')') {
        pos++;
    }
    npos = pos;
}

//Просто глубокое присваивание двух операторов - сначала освободили старую память, потом выделели новую
Sigma1d& Sigma1d::operator=(const Sigma1d &sg)
{
    p=sg.p;
    c=sg.c;
    return *this;
}

Sigma1d::Sigma1d(const Sigma1d &sg)
{
    p=sg.p;
    c=sg.c;
}
bool Sigma1d::operator==(const Sigma1d &sg)
{
    return sg.p == p && sg.c == c;
}
void Sigma1d::print()
{
    std::cout << "S";
    if (c==1) {
        std::cout << 'x';
    }
    if (c==2) {
        std::cout << 'y';
    }
    if (c==3) {
        std::cout << 'z';
    }
    std::cout << '(' << p << ')';
}
int Sigma1d::getType() const {
    return 1;
}

bool Sigma1d::operator<(const Sigma1d& other) const {
    return std::tie(p, c) < std::tie(other.p, other.c);
}

int Sigma2d::getPoint1() const {
    return p1;
}
int Sigma2d::getPoint2() const {
    return p2;
}
int Sigma2d::getC() const {
    return c;
}
void Sigma2d::setC(int c_)
{
    c=c_;
}
void Sigma2d::setPoint1(int p1_)
{
    p1=p1_;
}
void Sigma2d::setPoint2(int p2_)
{
    p2=p2_;
}
Sigma2d::Sigma2d(int p1_, int p2_, int c_)
{
    p1=p1_;
    p2=p2_;
    c=c_;
}
Sigma2d::Sigma2d(const std::string &s, int pos, int &npos)
{
    p1=1;
    p2=1;
    c=1;
    //Проверяет что строка начинается действительно с D или g и в случае успеха распознаем эти операторы (до аргументов), выделяем под них память и кладем в список
    if (s[pos] != 'S') {
        assert(0);
    }
    pos++;
        if (s[pos] == 'x') {
        c=1;
    }
        else{
            if (s[pos] == 'y') {
                c=2;
            }
            else{
                 if (s[pos] == 'z') {
                     c=3;
                 }
                 else{
                    assert(0);
                 }
            }
        }

    pos++;
    if (s[pos] != '(') {
        assert(0);
    }
    pos++;
    if (s[pos] >= '0' && s[pos] <= '9'){
        p1 = s[pos] - '0';
    }
    else{
        assert(0);
    }
    pos++;
    if (s[pos] == ',') {
        pos++;
    }
    if (s[pos] >= '0' && s[pos] <= '9'){
        p2 = s[pos] - '0';
    }
    else{
        assert(0);
    }
    pos++;
    if (s[pos] == ')') {
        pos++;
    }
    npos = pos;
}

//Просто глубокое присваивание двух операторов - сначала освободили старую память, потом выделели новую
Sigma2d& Sigma2d::operator=(const Sigma2d &sg)
{
    p1=sg.p1;
    p2=sg.p2;
    c=sg.c;
    return *this;
}

Sigma2d::Sigma2d(const Sigma2d &sg)
{
    p1=sg.p1;
    p2=sg.p2;
    c=sg.c;
}
bool Sigma2d::operator==(const Sigma2d &sg)
{
    return sg.p1 == p1 && sg.p2 == p2 && sg.c == c;
}
void Sigma2d::print()
{
    std::cout << "S";
    if (c==1) {
        std::cout << 'x';
    }
    if (c==2) {
        std::cout << 'y';
    }
    if (c==3) {
        std::cout << 'z';
    }
    std::cout << '(' << p1 << ',' << p2 << ')';
}
int Sigma2d::getType() const {
    return 2;
}

bool Sigma2d::operator<(const Sigma2d& other) const {
    return std::tie(p1, p2, c) < std::tie(other.p1, other.p2, other.c);
}

int Sigma3d::getPoint1()
{
    return p1;
}
void Sigma3d::setPoint(int p1_, int p2_, int p3_)
{
    p1=p1_;
    p2=p2_;
    p2=p2_;
}
Sigma3d::Sigma3d(int p1_, int p2_, int p3_, int c_)
{
    p1=p1_;
    p2=p2_;
    p2=p2_;
    c=c_;
}
Sigma3d::Sigma3d(const std::string &s, int pos, int &npos)
{
    p1=1;
    p2=1;
    p3=1;
    c=1;
    //Проверяет что строка начинается действительно с D или g и в случае успеха распознаем эти операторы (до аргументов), выделяем под них память и кладем в список
    if (s[pos] != 'S') {
        assert(0);
    }
    pos++;
        if (s[pos] == 'x') {
        c=1;
    }
        else{
            if (s[pos] == 'y') {
                c=2;
            }
            else{
                 if (s[pos] == 'z') {
                     c=3;
                 }
                 else{
                    assert(0);
                 }
            }
        }

    pos++;
    if (s[pos] != '(') {
        assert(0);
    }
    pos++;
    if (s[pos] >= '0' && s[pos] <= '9'){
        p1 = s[pos] - '0';
    }
    else{
        assert(0);
    }
    pos++;
    if (s[pos] == ',') {
        pos++;
    }
    if (s[pos] >= '0' && s[pos] <= '9'){
        p2 = s[pos] - '0';
    }
    else{
        assert(0);
    }
    pos++;
    if (s[pos] == ',') {
        pos++;
    }
    if (s[pos] >= '0' && s[pos] <= '9'){
        p3 = s[pos] - '0';
    }
    else{
        assert(0);
    }
    pos++;
    if (s[pos] == ')') {
        pos++;
    }
    npos = pos;
}

//Просто глубокое присваивание двух операторов - сначала освободили старую память, потом выделели новую
Sigma3d& Sigma3d::operator=(const Sigma3d &sg)
{
    p1=sg.p1;
    p2=sg.p2;
    p3=sg.p3;
    c=sg.c;
    return *this;
}

Sigma3d::Sigma3d(const Sigma3d &sg)
{
    p1=sg.p1;
    p2=sg.p2;
    p3=sg.p3;
    c=sg.c;
}
bool Sigma3d::operator==(const Sigma3d &sg)
{
    return sg.p1 == p1 && sg.p2 == p2 && sg.p3 == p3 && sg.c == c;
}
void Sigma3d::print()
{
    std::cout << "S";
    if (c==1) {
        std::cout << 'x';
    }
    if (c==2) {
        std::cout << 'y';
    }
    if (c==3) {
        std::cout << 'z';
    }
    std::cout << '(' << p1 << ',' << p2 << ',' << p3 << ')';
}
int Sigma3d::getType() const {
    return 3;
}

bool Sigma3d::operator<(const Sigma3d& other) const {
    return std::tie(p1, p2, p3, c) < std::tie(other.p1, other.p2, other.p3, other.c);
}

