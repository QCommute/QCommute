#include "Expression.h"
#include <cmath>
#include <fstream>
#include <cstdio>
#include <memory>
#include <thread>
#include <iostream>
#include <mutex>
#include <omp.h>
#include <algorithm>
#include <bitset>
#include "SerializationHelpers.h"

extern std::string directoryPath;
using namespace std;
//Expression самый главный класс, он включает в себя список элементов класса Sigma (строки Паули) и многочлен (список мономов)
//конструктор выражения от строки
Expression::Expression(const std::string& s)
{
    //Ну это ее при тебе писал, по первому символу вызываем нужные конструкторы, выделяем память и закидываем в список
    im = false;
    dim = 0;
    std::vector<Sigma1d> lst1;
    std::vector<Sigma2d> lst2;
    std::vector<Sigma3d> lst3;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == 'i') {
            im = true;
            i++;
        } else if (s[i] == '[') {
            size_t end = i + 1;
            while(end < s.size() && s[end] != ']')
                end++;
            assert(end < s.size());
            poly = Poly(s.substr(i, end - i + 1));
            i = end + 1;
        } else if (s[i] == 'S') {
            size_t tmp;
            int j=1;
            while (s[i+j] != ')'){
                j++;
            }
            if (j==4){
                lst1.emplace_back(s,i,tmp);
                dim = 1;
            }
            else{
                if (j==6){
                    lst2.emplace_back(s,i,tmp);
                    dim = 2;
                }
                else{
                    if (j==8){
                        lst3.emplace_back(s,i,tmp);
                        dim = 3;
                    }
                    else{
                        assert(0);
                    }
                }
            }
            i = tmp;
        } else {
            assert(0);
        }
    }

    if(!lst1.empty()) {
        assert(lst2.empty());
        assert(lst3.empty());
        setSigmas1D(lst1.data(), lst1.size());
    } else if(!lst2.empty()) {
        assert(lst1.empty());
        assert(lst3.empty());
        setSigmas2D(lst2.data(), lst2.size());
    } else if(!lst3.empty()) {
        assert(lst1.empty());
        assert(lst2.empty());
        setSigmas3D(lst3.data(), lst3.size());
    }
}

Expression::Expression(const std::vector<uint8_t>& data, size_t& ptr) {
    numSigmaX = deserializeUInt32(data, ptr);
    numSigmaY = deserializeUInt32(data, ptr);
    uint32_t numSigmaZ = deserializeUInt32(data, ptr);
    im = deserializeUInt8(data, ptr) == 1;
    dim = deserializeUInt8(data, ptr);
    for(size_t i = 0; i < (numSigmaX + numSigmaY + numSigmaZ) * dim; i++)
        sigmaCoords.push_back(deserializeInt32(data, ptr));
    poly = Poly(data, ptr);
}

Expression::Expression(const Sigma<1>* v, size_t vSize, const Poly& r, bool imm) {
    im = imm;
    dim = 1;
    setSigmas1D(v, vSize);
    poly = r;
}

Expression::Expression(const Sigma<2>* v, size_t vSize, const Poly& r, bool imm) {
    im = imm;
    dim = 2;
    setSigmas2D(v, vSize);
    poly = r;
}

Expression::Expression(const Sigma<3>* v, size_t vSize, const Poly& r, bool imm) {
    im = imm;
    dim = 3;
    setSigmas3D(v, vSize);
    poly = r;
}

void Expression::setSigmas1D(const Sigma<1>* sigmas, size_t sigmasSize) {
    numSigmaX = 0;
    numSigmaY = 0;
    sigmaCoords.clear();
    
    std::vector<Sigma1d> tmp(sigmas, sigmas + sigmasSize);
    std::sort(tmp.begin(), tmp.end(), [&](const Sigma1d& ptr1, const Sigma1d& ptr2) {
        if(ptr1.getC() != ptr2.getC())
            return ptr1.getC() < ptr2.getC();
        return ptr1.getPoint() < ptr2.getPoint();
    });
    for (int i = 0; i < (int)tmp.size(); i++) {
        sigmaCoords.push_back(tmp[i].getPoint().p[0]);
        if(tmp[i].getC() == 1)
            numSigmaX++;
        else if(tmp[i].getC() == 2)
            numSigmaY++;
    }
}

void Expression::setSigmas2D(const Sigma<2>* sigmas, size_t sigmasSize) {
    numSigmaX = 0;
    numSigmaY = 0;
    sigmaCoords.clear();
    
    std::vector<Sigma2d> tmp(sigmas, sigmas + sigmasSize);
    std::sort(tmp.begin(), tmp.end(), [&](const Sigma2d& ptr1, const Sigma2d& ptr2) {
        if(ptr1.getC() != ptr2.getC())
            return ptr1.getC() < ptr2.getC();
        return ptr1.getPoint() < ptr2.getPoint();
    });
    for (int i = 0; i < (int)tmp.size(); i++) {
        sigmaCoords.push_back(tmp[i].getPoint().p[0]);
        sigmaCoords.push_back(tmp[i].getPoint().p[1]);
        if(tmp[i].getC() == 1)
            numSigmaX++;
        else if(tmp[i].getC() == 2)
            numSigmaY++;
    }
}

void Expression::setSigmas3D(const Sigma<3>* sigmas, size_t sigmasSize) {
    numSigmaX = 0;
    numSigmaY = 0;
    sigmaCoords.clear();
    
    std::vector<Sigma3d> tmp(sigmas, sigmas + sigmasSize);
    std::sort(tmp.begin(), tmp.end(), [&](const Sigma3d& ptr1, const Sigma3d& ptr2) {
        if(ptr1.getC() != ptr2.getC())
            return ptr1.getC() < ptr2.getC();
        return ptr1.getPoint() < ptr2.getPoint();
    });
    for (int i = 0; i < (int)tmp.size(); i++) {
        sigmaCoords.push_back(tmp[i].getPoint().p[0]);
        sigmaCoords.push_back(tmp[i].getPoint().p[1]);
        sigmaCoords.push_back(tmp[i].getPoint().p[2]);
        if(tmp[i].getC() == 1)
            numSigmaX++;
        else if(tmp[i].getC() == 2)
            numSigmaY++;
    }
}

//много вспомогательных функций
template<>
std::vector<Sigma<1>> Expression::getList<1>() const {
    if(dim == 0)
        return {};
    assert(dim == 1);

    std::vector<Sigma<1>> res;
    for(size_t i = 0; i < sigmaCoords.size(); i++)
        res.push_back(Sigma<1>({sigmaCoords[i]}, (i < numSigmaX) ? 1 : ((i < numSigmaX + numSigmaY) ? 2 : 3)));
    return res;
}

template<>
std::vector<Sigma<2>> Expression::getList<2>() const {
    if(dim == 0)
        return {};
    assert(dim == 2);
    
    std::vector<Sigma<2>> res;
    for(size_t i = 0; i < sigmaCoords.size(); i+=2)
        res.push_back(Sigma<2>({sigmaCoords[i], sigmaCoords[i + 1]}, (i / 2 < numSigmaX) ? 1 : ((i / 2 < numSigmaX + numSigmaY) ? 2 : 3)));
    return res;
}

template<>
std::vector<Sigma<3>> Expression::getList<3>() const {
    if(dim == 0)
        return {};
    assert(dim == 3);

    std::vector<Sigma<3>> res;
    for(size_t i = 0; i < sigmaCoords.size(); i+=3)
        res.push_back(Sigma<3>({sigmaCoords[i], sigmaCoords[i + 1], sigmaCoords[i + 2]}, (i / 3 < numSigmaX) ? 1 : ((i / 3 < numSigmaX + numSigmaY) ? 2 : 3)));
    return res;
}

Poly& Expression::getPoly() {
    return poly;
}

const Poly& Expression::getPoly() const {
    return poly;
}

template<size_t D>
void commuteImpl(const Expression& O1, const Expression& O2,std::function<void(Expression&& newExpression)> callback) {
    //работа с полиномами, а именно их умножение
    Poly pol1 = O1.poly * O2.poly;
    pol1 *= 2;
    Poly m_pol1 = pol1 * (-1);
    
    //отсюда начинается сама коммутация в прицнипе тут осбо модно ничего не комментировать главное то как записывается результат в конце , там будет комментарий
    std::vector<Sigma<D>> lst = O1.getList<D>();
    std::sort(lst.begin(), lst.end(), [&](const Sigma<D>& s1, const Sigma<D>& s2) {
        return s1.getPoint() < s2.getPoint();
    });
    std::vector<Sigma<D>> lst1 = O2.getList<D>();
    std::sort(lst1.begin(), lst1.end(), [&](const Sigma<D>& s1, const Sigma<D>& s2) {
        return s1.getPoint() < s2.getPoint();
    });
    Sigma<D>* lst3 = new Sigma<D>[lst.size() + lst1.size()];
    for (int i = 0; i < (int)lst.size(); i++) {
        for (int j = 0; j < (int)lst1.size(); j++) {
            auto s1 = lst[i];
            auto s11 = lst1[j];

            if (s1.getC()==s11.getC()){
                continue;
            }
            int imm=0;
            int nc=LeviChivita(s1.getC(),s11.getC());
            imm++;
            if (O2.im==true){
                imm++;
            }
            if (O1.im==true){
                imm++;
            }
            if (nc<0){
                imm+=2;
                nc=nc*(-1);
            }

            // строим список сигм, отсортированный по точке
            // сигмы, попавшие в одну точку, схлопываются
            Point<D> pd = s11.getPoint() - s1.getPoint();
            Point<D> deltaLst1 = -Point<D>::elementwiseMin(pd, Point<D>()), deltaLst = Point<D>::elementwiseMax(Point<D>(), pd);
            size_t lst3Size = 0;
            size_t ptr = 0, ptr1 = 0;
            int nn = 0;
            while(ptr < lst.size() || ptr1 < lst1.size()) {
                if(ptr1 == j)
                    ptr1++;
                else {
                    Sigma<D> t;
                    if(ptr1 < lst1.size() && (ptr == lst.size() || (lst1[ptr1].getPoint() + deltaLst1 < lst[ptr].getPoint() + deltaLst))) {
                        t = lst1[ptr1];
                        t.setPoint(t.getPoint() + deltaLst1);
                        ptr1++;
                    } else {
                        t = lst[ptr];
                        if (ptr==i)
                            t.setC(nc);
                        t.setPoint(t.getPoint() + deltaLst);
                        ptr++;
                    }

                    if(lst3Size > 0 && lst3[lst3Size - 1].getPoint() == t.getPoint()) {
                        if(lst3[lst3Size - 1].getC() == t.getC())
                            lst3Size--;
                        else {
                            nn++;
                            int nc2 = LeviChivita(lst3[lst3Size - 1].getC(), t.getC());
                            imm++;
                            if (nc2 < 0) {
                                nc2 = -nc2;
                                imm += 2;
                            }
                            lst3[lst3Size - 1].setC(nc2);
                        }
                    } else
                        lst3[lst3Size++] = t;
                }
            }

            if ((nn % 2) != 0) {
                continue;
            }

            // ??? сдвигаем минимум в точку 1
            if(lst3Size > 0) {
                Point<D> mp = lst3[0].getPoint();
                for(size_t k = 1; k < lst3Size; k++)
                    mp = Point<D>::elementwiseMin(mp, lst3[k].getPoint());

                Point<D> delta;
                for(size_t k = 0; k < D; k++)
                    delta.p[k] = (mp.p[k] > 1) ? (-mp.p[k] + 1) : 0;
                for(size_t k = 0; k < lst3Size; k++)
                    lst3[k].setPoint(lst3[k].getPoint() + delta);
            }

            // ??? создаем новое выражение
            imm = imm % 4;
            if(imm == 2 || imm == 3) {
                Expression O1(lst3, lst3Size, m_pol1, (imm % 2) == 1);
                callback(std::move(O1));
            } else {
                Expression O1(lst3, lst3Size, pol1, (imm % 2) == 1);
                callback(std::move(O1));
            }
        }
    }
    delete[] lst3;
}

//самый главный оператор (коммутатор списка с элементов)
void Expression::Commute(const Expression& O, std::function<void(Expression&& newExpression)> callback) {
    if(sigmaCoords.empty() || O.sigmaCoords.empty())
        return;
    assert(dim == O.dim);
    assert(dim <= 3);

    if(dim == 1)
        commuteImpl<1>(*this, O, callback);
    else if(dim == 2)
        commuteImpl<2>(*this, O, callback);
    else
        commuteImpl<3>(*this, O, callback);
}

bool Expression::operator==(const Expression &ex1) const {
    if (im != ex1.im || numSigmaX != ex1.numSigmaX || numSigmaY != ex1.numSigmaY)
        return false;

    if(sigmaCoords.empty())
        return ex1.sigmaCoords.empty();
    if(ex1.sigmaCoords.empty())
        return sigmaCoords.empty();

    if(dim != ex1.dim)
        return false;
    // Сигмы хранятся в отсортированном порядке, поэтому достаточно сравнить вектора координат
    return sigmaCoords == ex1.sigmaCoords;
}

bool Expression::operator<(const Expression &ex1) const {
    if (im != ex1.im)
        return im;
    if(numSigmaX != ex1.numSigmaX)
        return numSigmaX < ex1.numSigmaX;
    if(numSigmaY != ex1.numSigmaY)
        return numSigmaY < ex1.numSigmaY;
    
    if(sigmaCoords.empty() != ex1.sigmaCoords.empty())
        return sigmaCoords.empty();

    if(dim != ex1.dim)
        return dim < ex1.dim;
    // Сигмы хранятся в отсортированном порядке, поэтому достаточно сравнить вектора координат
    return sigmaCoords < ex1.sigmaCoords;
}

int LeviChivita(int i, int j){
if (i==1){
    if (j==2){
        return 3;
    }
    if (j==3){
        return -2;
    }
}
if (i==2){
    if (j==3){
        return 1;
    }
    if (j==1){
        return -3;
    }
}
if (i==3){
    if (j==2){
        return -1;
    }
    if (j==1){
        return 2;
    }
}
return 0;
}

void Expression::print() const {
    if (im==true){
        std::cout << "i*";
    }
    poly.print();
    
    if(dim == 1) {
        auto lst1 = getList<1>();
        for(size_t i = 0; i < lst1.size(); i++) {
            std::cout << '*';
            lst1[i].print();
        }
    } else if(dim == 2) {
        auto lst2 = getList<2>();
        for(size_t i = 0; i < lst2.size(); i++) {
            std::cout << '*';
            lst2[i].print();
        }
    } else if(dim == 3) {
        auto lst3 = getList<3>();
        for(size_t i = 0; i < lst3.size(); i++) {
            std::cout << '*';
            lst3[i].print();
        }
    }
    std::cout << std::endl;
}

void Expression::serialize(std::vector<uint8_t>& res) const {
    serializeUInt32(res, numSigmaX);
    serializeUInt32(res, numSigmaY);
    serializeUInt32(res, sigmaCoords.size() / dim - numSigmaX - numSigmaY);
    serializeUInt8(res, im ? 1 : 0);
    serializeUInt8(res, dim);
    for(size_t i = 0; i < sigmaCoords.size(); i++)
        serializeInt32(res, sigmaCoords[i]);
    poly.serialize(res);
}

size_t ExpressionHasher::operator()(const Expression& expr) const {
    if(expr.sigmaCoords.empty())
        return expr.im ? 1 : 0;
    size_t res = expr.dim * 2 + (expr.im ? 1 : 0);
    for(size_t i = 0; i < expr.sigmaCoords.size(); i++)
        res = res * 0x159fa76bc + expr.sigmaCoords[i] + 1;
    return res;
}

