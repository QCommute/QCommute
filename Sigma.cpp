#include "Sigma.h"
#include <cstdlib>
#include <memory>

// Splitmix64, https://github.com/svaarala/duktape/blob/master/misc/splitmix64.c
uint64_t splitmix64(int x) {
    uint64_t z = x + 0x9e3779b97f4a7c15ull;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

extern int indcount;
extern int lorindcount;
int Sigma::getType() const
{
    return type;
}
Sigma::~Sigma()
{

}

 //описание класса Sigma тут особо комментировать нечего, разве что тут фактически три класса для одномерной, двумерной или трехмерной задачи
int Sigma1d::getPoint()
{
    return p;
}
int Sigma1d::getC()
{
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
    type = 1;
    p=p_;
    c=c_;
}
Sigma1d::Sigma1d(const std::string &s, int pos, int &npos)
{
    type = 1;
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
    type = 1;
    p=sg.p;
    c=sg.c;
    return *this;
}

Sigma1d::Sigma1d(const Sigma1d &sg)
{
    type = 1;
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
size_t Sigma1d::getHash() {
    return type ^ splitmix64(p) ^ c;
}
bool Sigma1d::operator<(const Sigma1d& other) const {
    return std::tie(p, c) < std::tie(other.p, other.c);
}
Sigma1d::~Sigma1d()
{

}

int Sigma2d::getPoint1()
{
    return p1;
}
int Sigma2d::getPoint2()
{
    return p2;
}
int Sigma2d::getC()
{
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
    type = 2;
    p1=p1_;
    p2=p2_;
    c=c_;
}
Sigma2d::Sigma2d(const std::string &s, int pos, int &npos)
{
    type = 2;
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
    type = 2;
    p1=sg.p1;
    p2=sg.p2;
    c=sg.c;
    return *this;
}

Sigma2d::Sigma2d(const Sigma2d &sg)
{
    type = 2;
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
size_t Sigma2d::getHash() {
    return type ^ splitmix64(p1) ^ splitmix64(p2) ^ c;
}
bool Sigma2d::operator<(const Sigma2d& other) const {
    return std::tie(p1, p2, c) < std::tie(other.p1, other.p2, other.c);
}
Sigma2d::~Sigma2d()
{

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
    type = 3;
    p1=p1_;
    p2=p2_;
    p2=p2_;
    c=c_;
}
Sigma3d::Sigma3d(const std::string &s, int pos, int &npos)
{
    type = 3;
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
    type = 3;
    p1=sg.p1;
    p2=sg.p2;
    p3=sg.p3;
    c=sg.c;
    return *this;
}

Sigma3d::Sigma3d(const Sigma3d &sg)
{
    type = 3;
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
size_t Sigma3d::getHash() {
    return type ^ splitmix64(p1) ^ splitmix64(p2) ^ splitmix64(p3) ^ c;
}
bool Sigma3d::operator<(const Sigma3d& other) const {
    return std::tie(p1, p2, p3, c) < std::tie(other.p1, other.p2, other.p3, other.c);
}
Sigma3d::~Sigma3d()
{

}




void deepCopyBase(std::shared_ptr<Sigma> &to, std::shared_ptr<Sigma> from)
{
    int name = from->getType();
    if (name == 1) {
        to = std::make_shared<Sigma1d>(*std::dynamic_pointer_cast<Sigma1d>(from));
    } else if (name == 2) {
        to = std::make_shared<Sigma2d>(*std::dynamic_pointer_cast<Sigma2d>(from));
    } else if (name == 3) {
        to = std::make_shared<Sigma3d>(*std::dynamic_pointer_cast<Sigma3d>(from));
    } else {
        std::cout << "name = " << name << std::endl;
        to = nullptr;
        assert(0);
    }
}
