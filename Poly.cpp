#include "Poly.h"
#include <gmp-x86_64.h>

void Poly::addMonom(std::shared_ptr<Monom> monom) {
    data_.push_back(monom);
}

Poly::Poly(const std::string& str) {
    assert(str[0] == '[');
    assert(str.back() == ']');

    size_t cur = 0;
    while(str[cur] != ']') {
        assert(str[cur] == ';' || str[cur] == '[');
        size_t next = cur + 1;
        while(str[next] != ';' && str[next] != ']')
            next++;
        data_.push_back(std::make_shared<Monom>(str.substr(cur + 1, next - cur - 1)));
        cur = next;
    }
}

std::vector<std::shared_ptr<Monom>> Poly::getData() {
    return data_;
}

bool Poly::isZero() const {
    for (int k= 0; k < (int)data_.size(); k++) {
        if (data_[k]->isZero()==false){
            return false;
        }
    }
    return true;
}

void Poly::print() const {
    std::cout << '[';
    for (int i = 0; i < (int)data_.size(); i++) {
        if (i){
            std::cout << ';';
        }
        data_[i]->print();
    }
    std::cout << ']';
}

void Poly::deepCopyFrom(const Poly& other) {
    for (int i = 0; i < (int)other.data_.size(); i++) {
        std::shared_ptr<Monom> t;
        deepCopyMonom(t, other.data_[i]);
        addMonom(t);
    }
}

Poly& Poly::operator+=(const Poly& other) {
    const std::vector<std::shared_ptr<Monom>>& added = other.data_;
    std::vector<bool> goodMonom(added.size(), true);
    for (int l = 0; l < (int)added.size();l++) {
        for (int k= 0; k < data_.size(); k++) {
            if (*data_[k]==*added[l]){
                data_[k]->Sum(*added[l]);
                goodMonom[l] = false;
                break;
            }
        }
    }
    for (int l = 0; l < (int)added.size(); l++) {
        if(goodMonom[l])
            addMonom(added[l]);
    }
    return *this;
}

Poly Poly::operator*(const Poly& other) const {
    Poly res;
    for(int32_t i = 0; i < data_.size(); i++) {
        Poly tmp;
        tmp.deepCopyFrom(other);
        for(int32_t j = 0; j < tmp.data_.size(); j++)
            tmp.data_[j]->Prod(*(data_[i]));
        res += tmp;
    }
    return res;
}

bool Poly::operator==(const Poly& other) const {
    if(data_.size() != other.data_.size())
        return false;
    std::vector<bool> otherUsed(data_.size(), false);
    for(size_t i = 0; i < data_.size(); i++) {
        for(size_t j = 0; j < other.data_.size(); j++)
            if(!otherUsed[j] && (*data_[i]) == (*other.data_[j])) {
                mpz_t coef_i, coef_j;
                mpz_init(coef_i);
                mpz_init(coef_j);
                data_[i]->getCoef(coef_i);
                other.data_[j]->getCoef(coef_j);
                if(mpz_cmp(coef_i, coef_j) == 0) {
                    otherUsed[j] = true;
                    break;
                }
            }
    }
    for(size_t i = 0; i < data_.size(); i++)
        if(!otherUsed[i])
            return false;
    return true;
}