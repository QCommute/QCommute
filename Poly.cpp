#include "Poly.h"

void Poly::addMonom(std::shared_ptr<Monom> monom) {
    data_.push_back(monom);
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