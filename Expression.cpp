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
    std::vector<std::shared_ptr<Sigma>> lst;
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
            int tmp;
            int j=1;
            while (s[i+j] != ')'){
                j++;
            }
            if (j==4){
                std::shared_ptr<Sigma1d> ptr(new Sigma1d(s,i,tmp));
                lst.push_back(ptr);
                dim = 1;
            }
            else{
                if (j==6){
                    std::shared_ptr<Sigma2d> ptr(new Sigma2d(s,i,tmp));
                    lst.push_back(ptr);
                    dim = 2;
                }
                else{
                    if (j==8){
                        std::shared_ptr<Sigma3d> ptr(new Sigma3d(s,i,tmp));
                        lst.push_back(ptr);
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
    
    if(dim == 1) {
        std::vector<Sigma1d> lst2;
        for(size_t i = 0; i < lst.size(); i++)
            lst2.push_back(*std::dynamic_pointer_cast<Sigma1d>(lst[i]));
        setSigmas1D(lst2);
    } else if(dim == 2) {
        std::vector<Sigma2d> lst2;
        for(size_t i = 0; i < lst.size(); i++)
            lst2.push_back(*std::dynamic_pointer_cast<Sigma2d>(lst[i]));
        setSigmas2D(lst2);
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
    //std::cerr << "HERE" << " " << ptr << "\n";
    poly = Poly(data, ptr);
}

Expression::Expression(const std::vector<Sigma1d> &v, const Poly& r, bool imm) {
    im = imm;
    dim = 1;
    setSigmas1D(v);
    poly = r;
}

Expression::Expression(const std::vector<Sigma2d> &v, const Poly& r, bool imm) {
    im = imm;
    dim = 2;
    setSigmas2D(v);
    poly = r;
}

void Expression::setSigmas1D(const std::vector<Sigma1d>& sigmas) {
    numSigmaX = 0;
    numSigmaY = 0;
    sigmaCoords.clear();
    
    auto tmp = sigmas;
    std::sort(tmp.begin(), tmp.end(), [&](const Sigma1d& ptr1, const Sigma1d& ptr2) {
        if(ptr1.getC() != ptr2.getC())
            return ptr1.getC() < ptr2.getC();
        return ptr1.getPoint() < ptr2.getPoint();
    });
    for (int i = 0; i < (int)tmp.size(); i++) {
        sigmaCoords.push_back(tmp[i].getPoint());
        if(tmp[i].getC() == 1)
            numSigmaX++;
        else if(tmp[i].getC() == 2)
            numSigmaY++;
    }
}

void Expression::setSigmas2D(const std::vector<Sigma2d>& sigmas) {
    numSigmaX = 0;
    numSigmaY = 0;
    sigmaCoords.clear();
    
    auto tmp = sigmas;
    std::sort(tmp.begin(), tmp.end(), [&](const Sigma2d& ptr1, const Sigma2d& ptr2) {
        if(ptr1.getC() != ptr2.getC())
            return ptr1.getC() < ptr2.getC();
        if(ptr1.getPoint1() != ptr2.getPoint1())
            return ptr1.getPoint1() < ptr2.getPoint1();
        return ptr1.getPoint2() < ptr2.getPoint2();
    });
    for (int i = 0; i < (int)tmp.size(); i++) {
        sigmaCoords.push_back(tmp[i].getPoint1());
        sigmaCoords.push_back(tmp[i].getPoint2());
        if(tmp[i].getC() == 1)
            numSigmaX++;
        else if(tmp[i].getC() == 2)
            numSigmaY++;
    }
}

//много вспомогательных функций
std::vector<std::shared_ptr<Sigma>> Expression::getList() const
{
    std::vector<std::shared_ptr<Sigma>> res;
    if(dim == 1) {
        for(size_t i = 0; i < sigmaCoords.size(); i++)
            res.push_back(std::make_shared<Sigma1d>(sigmaCoords[i], (i < numSigmaX) ? 1 : ((i < numSigmaX + numSigmaY) ? 2 : 3)));
    } else if(dim == 2) {
        for(size_t i = 0; i < sigmaCoords.size(); i+=2)
            res.push_back(std::make_shared<Sigma2d>(sigmaCoords[i], sigmaCoords[i + 1], (i / 2 < numSigmaX) ? 1 : ((i / 2 < numSigmaX + numSigmaY) ? 2 : 3)));
    } else {
        for(size_t i = 0; i < sigmaCoords.size(); i+=3)
            res.push_back(std::make_shared<Sigma3d>(sigmaCoords[i], sigmaCoords[i + 1], sigmaCoords[i + 2], (i / 3 < numSigmaX) ? 1 : ((i / 3 < numSigmaX + numSigmaY) ? 2 : 3)));
    }
    return res;
}

std::vector<Sigma1d> Expression::getList1D() const {
    if(dim == 0)
        return {};
    assert(dim == 1);
    
    std::vector<Sigma1d> res;
    for(size_t i = 0; i < sigmaCoords.size(); i++)
        res.emplace_back(sigmaCoords[i], (i < numSigmaX) ? 1 : ((i < numSigmaX + numSigmaY) ? 2 : 3));
    return res;
}

Poly& Expression::getPoly()
{
    return poly;
}
const Poly& Expression::getPoly() const
{
    return poly;
}

//самый главный оператор (коммутатор списка с элементов)
void Expression::Commute(std::vector <Expression> &O1,  const Expression &O)
{
    if(sigmaCoords.empty() || O.sigmaCoords.empty())
        return;
    assert(dim == O.dim);

    //работа с полиномами, а именно их умножение
    Poly pol1 = poly * O.poly;
    pol1 *= 2;
    
    //отсюда начинается сама коммутация в прицнипе тут осбо модно ничего не комментировать главное то как записывается результат в конце , там будет комментарий
    if(dim == 1) {
        std::vector<Sigma1d> lst = getList1D();
        std::sort(lst.begin(), lst.end(), [&](const Sigma1d& s1, const Sigma1d& s2) {
            return s1.getPoint() < s2.getPoint();
        });
        std::vector<Sigma1d> lst1 = O.getList1D();
        std::sort(lst1.begin(), lst1.end(), [&](const Sigma1d& s1, const Sigma1d& s2) {
            return s1.getPoint() < s2.getPoint();
        });
        Sigma1d* lst3 = new Sigma1d[lst.size() + lst1.size()];
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
                if (O.im==true){
                    imm++;
                }
                 if (im==true){
                    imm++;
                }
                if (nc<0){
                    imm+=2;
                    nc=nc*(-1);
                }

                // строим список сигм, отсортированный по точке
                int pd=s11.getPoint()-s1.getPoint();
                int deltaLst1 = -std::min(pd, 0), deltaLst = std::max(0, pd);
                size_t lst3Size = 0;
                size_t ptr = 0, ptr1 = 0;
                while(ptr < lst.size() || ptr1 < lst1.size()) {
                    if(ptr1 == j)
                        ptr1++;
                    else if(ptr1 < lst1.size() && (ptr == lst.size() || (lst1[ptr1].getPoint() + deltaLst1 < lst[ptr].getPoint() + deltaLst))) {
                        Sigma1d t = lst1[ptr1];
                        t.setPoint(t.getPoint() + deltaLst1);
                        lst3[lst3Size++] = t;
                        ptr1++;
                    } else {
                        Sigma1d t = lst[ptr];
                        if (ptr==i)
                            t.setC(nc);
                        t.setPoint(t.getPoint() + deltaLst);
                        lst3[lst3Size++] = t;
                        ptr++;
                    }
                }

                // ??? схлопываем сигмы, попавшие в одну точку
                int nn=0;
                std::vector<Sigma1d> lst32;
                for(size_t k = 0; k < lst3Size; k++) {
                    if(!lst32.empty() && lst32.back().getPoint() == lst3[k].getPoint()) {
                        if(lst32.back().getC() == lst3[k].getC())
                            lst32.pop_back();
                        else {
                            nn++;
                            int nc2 = LeviChivita(lst32.back().getC(), lst3[k].getC());
                            imm++;
                            if (nc2 < 0) {
                                nc2 = -nc2;
                                imm += 2;
                            }
                            lst32.back().setC(nc2);
                        }
                    } else
                        lst32.push_back(lst3[k]);
                }
                if ((nn % 2) != 0) {
                    continue;
                }

                // ??? сдвигаем минимум в точку 1
                if(!lst32.empty()) {
                    int mp = lst32[0].getPoint();
                    for(size_t k = 1; k < lst32.size(); k++)
                        mp = std::min(mp, lst32[k].getPoint());

                    int delta = (mp > 1) ? (-mp + 1) : 0;
                    for(size_t k = 0; k < lst32.size(); k++)
                        lst32[k].setPoint(lst32[k].getPoint() + delta);
                }

                // ??? создаем новое выражение
                imm = imm % 4;
                if(imm == 2 || imm == 3)
                    O1.emplace_back(lst32, pol1 * (-1), (imm % 2) == 1);
                else
                    O1.emplace_back(lst32, pol1, (imm % 2) == 1);
            }
        }
        delete[] lst3;
    } else if(dim == 2) {
        std::vector<std::shared_ptr<Sigma>> lst = getList();
        std::vector<std::shared_ptr<Sigma>> lst1 = O.getList();
        for (int i = 0; i < (int)lst.size(); i++) {
            for (int j = 0; j < (int)lst1.size(); j++){
                if (lst[i]->getType()!=lst1[j]->getType()){
                    continue;
                }
                auto s2 = std::dynamic_pointer_cast<Sigma2d>(lst[i]);
                auto s21 = std::dynamic_pointer_cast<Sigma2d>(lst1[j]);
                
                if (s2->getC()==s21->getC()){
                    continue;
                }
                int imm=0;
                int nc=LeviChivita(s2->getC(),s21->getC());
                imm++;
                if (O.im==true){
                    imm++;
                }
                if (im==true){
                    imm++;
                }
                if (nc<0){
                    imm+=2;
                    nc=nc*(-1);
                }
                int pd1=s21->getPoint1()-s2->getPoint1();
                int pd2=s21->getPoint2()-s2->getPoint2();
                std::vector<Sigma2d> lst3;
                for (int k = 0; k < (int)lst.size(); k++) {
                    Sigma2d t = *std::dynamic_pointer_cast<Sigma2d>(lst[k]);
                    if (k==i){
                        t.setC(nc);
                    }
                    if (pd1>0){
                        t.setPoint1(t.getPoint1()+pd1);
                    }
                    if (pd2>0){
                        t.setPoint2(t.getPoint2()+pd2);
                    }
                    lst3.push_back(t);
                }
                for (int k = 0; k < (int)lst1.size(); k++) {
                    if (k==j){
                        continue;
                    }
                    Sigma2d t = *std::dynamic_pointer_cast<Sigma2d>(lst1[k]);
                    if (pd1<0){
                        t.setPoint1(t.getPoint1()-pd1);
                    }
                    if (pd2<0){
                        t.setPoint2(t.getPoint2()-pd2);
                    }
                    lst3.push_back(t);
                }

                // ??? схлопываем сигмы, попавшие в одну точку
                int nn=0;
                std::sort(lst3.begin(), lst3.end(), [](const Sigma2d& s1, const Sigma2d& s2) {
                    return s1 < s2;
                });
                std::vector<Sigma2d> lst32;
                for(size_t k = 0; k < lst3.size(); k++) {
                    if(!lst32.empty() && lst32.back().getPoint1() == lst3[k].getPoint1() && lst32.back().getPoint2() == lst3[k].getPoint2()) {
                        if(lst32.back().getC() == lst3[k].getC())
                            lst32.pop_back();
                        else {
                            nn++;
                            int nc2 = LeviChivita(lst32.back().getC(), lst3[k].getC());
                            imm++;
                            if (nc2 < 0) {
                                nc2 = -nc2;
                                imm += 2;
                            }
                            lst32.back().setC(nc2);
                        }
                    } else
                        lst32.push_back(lst3[k]);
                }
                lst3 = lst32;
                if ((nn % 2) != 0){
                    continue;
                }

                // ??? сдвигаем минимум в точку (1, 1)
                if(!lst3.empty()) {
                    int mp1 = lst3[0].getPoint1();
                    int mp2 = lst3[0].getPoint2();
                    for(size_t k = 1; k < lst3.size(); k++) {
                        mp1 = std::min(mp1, lst3[k].getPoint1());
                        mp2 = std::min(mp2, lst3[k].getPoint2());
                    }

                    int delta1 = (mp1 > 1) ? (-mp1 + 1) : 0;
                    int delta2 = (mp2 > 1) ? (-mp2 + 1) : 0;
                    for(size_t k = 0; k < lst3.size(); k++) {
                        lst3[k].setPoint1(lst3[k].getPoint1() + delta1);
                        lst3[k].setPoint2(lst3[k].getPoint2() + delta2);
                    }
                }

                // ??? создаем новое выражение
                imm = imm % 4;
                if(imm == 2 || imm == 3)
                    O1.emplace_back(lst3, pol1 * (-1), (imm % 2) == 1);
                else
                    O1.emplace_back(lst3, pol1, (imm % 2) == 1);
            }
        }
    }
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
    auto lst = getList();
    
    if (im==true){
        std::cout << "i*";
    }
    poly.print();
    std::cout << '*';
    for (int i = 0; i < (int)lst.size(); i++) {
        lst[i]->print();

        if (i != (int)lst.size() - 1) {
            std::cout << '*';
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

