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
extern int indcount;
extern int lorindcount;
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
        return ptr1.getC() < ptr2.getC();
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
        return ptr1.getC() < ptr2.getC();
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
Poly& Expression::getPoly()
{
    return poly;
}
const Poly& Expression::getPoly() const
{
    return poly;
}
int Expression::getLength() const
{
    return (int)getList().size();
}
//самый главный оператор (коммутатор списка с элементов)
void Expression::Commute(std::vector <Expression> &O1,  const Expression &O)
{
    //работа с полиномами, а именно их умножение
    Poly pol1 = poly * O.poly;
    pol1 *= 2;

    //отсюда начинается сама коммутация в прицнипе тут осбо модно ничего не комментировать главное то как записывается результат в конце , там будет комментарий
    std::vector<std::shared_ptr<Sigma>> lst = getList();
    std::vector<std::shared_ptr<Sigma>> lst1 = O.getList();
    for (int i = 0; i < (int)lst.size(); i++) {
        for (int j = 0; j < (int)lst1.size(); j++){
            if (lst[i]->getType()!=lst1[j]->getType()){
                continue;
            }
            auto s1 = std::dynamic_pointer_cast<Sigma1d>(lst[i]);
            auto s2 = std::dynamic_pointer_cast<Sigma2d>(lst[i]);
            auto s3 = std::dynamic_pointer_cast<Sigma3d>(lst[i]);
            auto s11 = std::dynamic_pointer_cast<Sigma1d>(lst1[j]);
            auto s21 = std::dynamic_pointer_cast<Sigma2d>(lst1[j]);
            auto s31 = std::dynamic_pointer_cast<Sigma3d>(lst1[j]);
            if (s1) {
                if (s1->getC()==s11->getC()){
                    continue;
                }
                int imm=0;
                int nc=LeviChivita(s1->getC(),s11->getC());
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
                int pd=s11->getPoint()-s1->getPoint();
                std::vector<Sigma1d> lst3;
                for (int k = 0; k < (int)lst.size(); k++) {
                    Sigma1d t = *std::dynamic_pointer_cast<Sigma1d>(lst[k]);
                    if (k==i){
                        t.setC(nc);
                    }
                    if (pd>0){
                        t.setPoint(t.getPoint()+pd);
                    }
                    lst3.push_back(t);
                }
                for (int k = 0; k < (int)lst1.size(); k++) {
                    if (k==j){
                        continue;
                    }
                    Sigma1d t = *std::dynamic_pointer_cast<Sigma1d>(lst1[k]);
                    if (pd<0){
                        t.setPoint(t.getPoint()-pd);
                    }
                    lst3.push_back(t);
                }

                // ??? схлопываем сигмы, попавшие в одну точку
                int nn=0;
                std::sort(lst3.begin(), lst3.end(), [](const Sigma1d& s1, const Sigma1d& s2) {
                    return s1 < s2;
                });
                std::vector<Sigma1d> lst32;
                for(size_t k = 0; k < lst3.size(); k++) {
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
                lst3 = lst32;
                if ((nn % 2) != 0) {
                    continue;
                }

                // ??? сдвигаем минимум в точку 1
                int mp=0;
                for(size_t k = 0; k < lst3.size(); k++) {
                    if (mp == 0){
                        mp = lst3[k].getPoint();
                    }
                    else
                        mp = std::min(mp, lst3[k].getPoint());
                }
                if (mp>1){
                    for (int k = 0; k < (int)lst3.size(); k++)
                        lst3[k].setPoint(lst3[k].getPoint()-mp+1);
                }

                // ??? создаем новое выражение
                imm = imm % 4;
                O1.emplace_back(lst3, pol1 * ((imm == 2 || imm == 3) ? (-1) : 1), (imm % 2) == 1);
            }
            if (s2){
                bool b=false;
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
                int mp1=0;
                int mp2=0;
                for(size_t k = 0; k < lst3.size(); k++) {
                    if (mp1==0){
                        mp1=lst3[k].getPoint1();
                    }
                    else
                        mp1 = std::min(mp1, lst3[k].getPoint1());
                    if (mp2==0){
                        mp2=lst3[k].getPoint2();
                    }
                    else
                        mp2 = std::min(mp2, lst3[k].getPoint2());
                }
                if (mp1>1){
                    for (int k = 0; k < (int)lst3.size(); k++)
                        lst3[k].setPoint1(lst3[k].getPoint1()-mp1+1);
                }
                if (mp2>1){
                    for (int k = 0; k < (int)lst3.size(); k++)
                        lst3[k].setPoint2(lst3[k].getPoint2()-mp2+1);
                }

                // ??? создаем новое выражение
                imm = imm % 4;
                Expression n_exp(lst3, pol1 * ((imm == 2 || imm == 3) ? (-1) : 1), (imm % 2) == 1); //это строчка с коэффициентов, таких утт создается много, все они в целом дают результат 
                
                uint8_t Tl= n_exp.TotalLength();
                uint8_t Mp= n_exp.MaxPoint();
                mpz_t mbr_x;
                mpz_t mbr_y;
                mpz_t mbr_z;
                mpz_init(mbr_x);
                mpz_init(mbr_y);
                mpz_init(mbr_z);
                n_exp.MakeMbr(mbr_x, mbr_y, mbr_z, Mp); //Вот тут создаем как раз те самые числа
                int numBitsx = mpz_sizeinbase(mbr_x, 2);
                int numBitsy = mpz_sizeinbase(mbr_y, 2);
                int numBitsz = mpz_sizeinbase(mbr_z, 2);
                uint8_t numBytesx = (numBitsx + 7) / 8; // округляем в большую сторону
                uint8_t numBytesy = (numBitsy + 7) / 8; // округляем в большую сторону
                uint8_t numBytesz = (numBitsz + 7) / 8; // округляем в большую сторону
                char* bufferx = new char[numBytesx];
                char* buffery = new char[numBytesy];
                char* bufferz = new char[numBytesz];
                mpz_export(bufferx, nullptr, 1, 1, 0, 0, mbr_x);
                mpz_export(buffery, nullptr, 1, 1, 0, 0, mbr_y);
                mpz_export(bufferz, nullptr, 1, 1, 0, 0, mbr_z);
                #pragma omp critical
                 {
                    //cout << mbr_x<< "ddwdw" << mbr_y << endl;
                    //создаем файл с соответвующим названием и помещаем в него эти числа
                    std::ofstream ofs(directoryPath+"/"+std::to_string(Mp)+"_"+std::to_string(Tl)+".bin", std::ios::binary);
                    ofs.write(reinterpret_cast<char*>(&numBytesx), sizeof(numBytesx));                      
                    ofs.write(bufferx, numBytesx);
                    ofs.write(reinterpret_cast<char*>(&numBytesx), sizeof(numBytesx));                      
                    ofs.write(buffery, numBytesx);
                    ofs.write(reinterpret_cast<char*>(&numBytesx), sizeof(numBytesx));                      
                    ofs.write(bufferz, numBytesx);
                    ofs.close(); //пока я эти файлы не использую, не успел ещё это сделать, просто в них скалдываем, еще нужно скалдывать в файлы полиномы, пока тоже не реализовано
                    O1.push_back(n_exp); //так что пока кладем в опретивную память и создаем новый список
                 }
                delete[] bufferx;
                delete[] buffery;
                delete[] bufferz;
             }
         }
    }
}

uint8_t Expression::TotalLength()
{
    auto lst = getList();
    
    return lst.size();
}
uint8_t Expression::MaxPoint()
{
    auto lst = getList();
    
    uint8_t mp=0;
    for (int i = 0; i < (int)lst.size(); i++){
        auto s1 = std::dynamic_pointer_cast<Sigma1d>(lst[i]);
        auto s2 = std::dynamic_pointer_cast<Sigma2d>(lst[i]);
        auto s3 = std::dynamic_pointer_cast<Sigma3d>(lst[i]);
        if (s1){
            if (s1->getPoint()>mp){
                mp=s1->getPoint();
            }
        }
        if (s2){
            if (s2->getPoint1()>mp){
                mp=s2->getPoint1();
            }
            if (s2->getPoint2()>mp){
                mp=s2->getPoint2();
            }
        }
    }
    return mp;
}

void Expression::MakeMbr(mpz_t mbr_x, mpz_t mbr_y, mpz_t mbr_z, uint8_t Mp)//функция которая создает три числа из строчки Паули
{
    auto lst = getList();
    
    for (int i = 0; i < (int)lst.size(); i++){
        auto s2 = std::dynamic_pointer_cast<Sigma2d>(lst[i]);
        if (s2->getC()==1){
            mpz_setbit(mbr_x, Mp*(s2->getPoint1()-1)+s2->getPoint2()-1);
        }
        if (s2->getC()==2){
            mpz_setbit(mbr_y, Mp*(s2->getPoint1()-1)+s2->getPoint2()-1);
        }
        if (s2->getC()==3){
            mpz_setbit(mbr_z, Mp*(s2->getPoint1()-1)+s2->getPoint2()-1);
        }
    }
}


bool Expression::operator==(const Expression &ex1) const {
    if (im != ex1.im || numSigmaX != ex1.numSigmaX || numSigmaY != ex1.numSigmaY)
        return false;

    auto lst = getList();
    auto lst1 = ex1.getList();
    
    if (lst1.size() != lst.size())
        return false;

    std::vector<bool> used2(lst.size(), false);
    for(int32_t i = 0; i < lst.size(); i++) {
        for(int32_t k = 0; k <= lst.size(); k++) {
            if(k == lst.size())
                return false;
            if(used2[k])
                continue;
            if (lst[i]->getType() == lst1[k]->getType()) {
                auto s1 = std::dynamic_pointer_cast<Sigma1d>(lst[i]);
                auto s2 = std::dynamic_pointer_cast<Sigma2d>(lst[i]);
                auto s3 = std::dynamic_pointer_cast<Sigma3d>(lst[i]);
                auto s11 = std::dynamic_pointer_cast<Sigma1d>(lst1[k]);
                auto s21 = std::dynamic_pointer_cast<Sigma2d>(lst1[k]);
                auto s31 = std::dynamic_pointer_cast<Sigma3d>(lst1[k]);
                if (s1){
                    if (*s11==*s1){
                        used2[k] = true;
                        break;
                    }
                }
                if (s2){
                    if (*s21==*s2){
                        used2[k] = true;
                        break;
                    }
                }
                if (s3){
                    if (*s31==*s3){
                        used2[k] = true;
                        break;
                    }
                }
            }
        }
    }
    return true;
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

int PrimeNumber(int n){
    int k=0;
    long int test=1;
    while(k<n){
          test++;
          if (prime(test)==true){
            k++;
          }
    }
    return test;
}
bool prime(long int n){
	for(long int i=2;i<=sqrt(n);i++)
		if(n%i==0)
			return false;
	return true;
}
//std::vector<int> Expression::getCoeff() const
//{
 //   return coef;
//}
void Expression::print()
{
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

size_t ExpressionHasher::operator()(const Expression& expr) const {
    auto sigmaCompare = [](const std::shared_ptr<Sigma>& s1_ptr, const std::shared_ptr<Sigma>& s2_ptr) {
        if(s1_ptr->getType() != s2_ptr->getType())
            return s1_ptr->getType() < s2_ptr->getType();
        auto s1 = std::dynamic_pointer_cast<Sigma1d>(s1_ptr);
        auto s2 = std::dynamic_pointer_cast<Sigma2d>(s1_ptr);
        auto s3 = std::dynamic_pointer_cast<Sigma3d>(s1_ptr);
        auto s11 = std::dynamic_pointer_cast<Sigma1d>(s2_ptr);
        auto s21 = std::dynamic_pointer_cast<Sigma2d>(s2_ptr);
        auto s31 = std::dynamic_pointer_cast<Sigma3d>(s2_ptr);
        if (s1)
            return *s1 < *s11;
        if (s2)
            return *s2 < *s21;
        if (s3)
            return *s3 < *s31;
    };
    auto lst2 = expr.getList();
    size_t res = lst2.size() * 2 + (expr.im ? 1 : 0);
    std::sort(lst2.begin(), lst2.end(), sigmaCompare);

    for(size_t i = 0; i < lst2.size(); i++)
        res = res * 0x159fa76bc + lst2[i]->getHash();
    return res;
}
