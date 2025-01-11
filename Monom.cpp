#include "Monom.h"
#include <cstdlib>
#include <gmpxx.h>

//класс мономов, в прицнипе я думал переписать его и сделать класс полиномов, но пока не сделал.
//моном содежит список степеней и коэффициент. в списке степеней каждому элементу соответвует степень некого параметра, число параметров в принципе не ограничено

Monom::Monom(const std::string& s) {
    mpz_init(coef);
    size_t pos = 0;
    while (pos < s.size()) {
        if (s[pos]==','){
            pos++;
        }
        std::string nums = "";
        while ((s[pos] >= '0' && s[pos] <= '9') || (s[pos] == '-')){
            nums.push_back(s[pos]);
            pos++;
        }
        
        if (mpz_cmp_si(coef, 0)==0 && (int)pows.size()==0){
            mpz_set_str(coef, nums.c_str(), 10);
        } else {
            pows.push_back(std::stoi(nums));
        }
    }
}

Monom::Monom(const Monom& other) {
    mpz_init(coef);
    pows = other.pows;
    mpz_set(coef, other.coef);
}

Monom& Monom::operator=(const Monom& other) {
    if(this == &other)
        return *this;
    pows = other.pows;
    mpz_set(coef, other.coef);
    return *this;
}

Monom::Monom(Monom&& other) {
    mpz_init(coef);
    pows = std::move(other.pows);
    mpz_set(coef, other.coef);
}

Monom& Monom::operator=(Monom&& other) {
    if(this == &other)
        return *this;
    pows = std::move(other.pows);
    mpz_set(coef, other.coef);
    return *this;
}

bool Monom::operator==(const Monom& other) const {
    if(pows != other.pows)
        return false;
    return mpz_cmp(coef, other.coef) == 0;
}

bool Monom::isZero() const {
    return mpz_cmp_si(coef, 0) == 0;
}

bool Monom::samePowers(const Monom& other) const {
    return pows == other.pows;
}

bool Monom::lessPowers(const Monom& other) const {
    return pows < other.pows;
}

void Monom::Sum(const Monom& mn) {
    mpz_add(coef, coef, mn.coef);
}

Monom& Monom::operator*=(const Monom& other) {
    mpz_mul(coef, coef, other.coef);
    for (size_t i = 0; i < other.pows.size(); i++) {
        if(i >= pows.size())
            pows.push_back(other.pows[i]);
        else
            pows[i] += other.pows[i];
    }
    return *this;
}

Monom& Monom::operator*=(int n) {
    mpz_mul_si(coef, coef, n);
    return *this;
}

void Monom::setCoef(mpz_t c_) {
    mpz_set(coef,c_);
}

void Monom::getCoef(mpz_t& res) const {
    mpz_set(res, coef);
}

void Monom::print() const {
    mpz_out_str(stdout, 10, coef);
    for (size_t i = 0; i < pows.size(); i++)
        std::cout << ',' << +(pows[i]);
}

Monom::~Monom() {
   mpz_clear(coef);
}

