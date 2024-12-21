#include <vector>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <string>
#include <memory>
#include <iostream>
#include <cassert>
#include <ctime>
#include <typeinfo>
#include <typeindex>
#include <cmath>
#include <cstdio>
#include <bitset>
#include "Expression.h"
#include "Sigma.h"
//#include <conio.h>
#include <thread>
#include <mutex>
#include <omp.h>
#include <stdio.h>
#include <gmpxx.h>
#include <filesystem>
//#include <ginac/ginac.h>
int indcount;
int lorindcount;
std::string directoryPath;
typedef std::chrono::high_resolution_clock Clock;

using namespace std;

int raiseToPow(int x, int power)
{
    int result;
    int i;
    result =1;
    for (i=1; i<=power;i++)
    {
        result = result*x;
    }
    return(result);
}



int
main(int argc, char** argv)
{
    directoryPath="./tmp"; //место где хранится файлы с коммутаторами
    //std::cout << sizeof(Sigma2d)<< std::endl;
    auto start_time = Clock::now();//просто часы чтобы мерить время исполнения
    setlocale(LC_ALL, "");
    std::vector <Expression> H;//Переменная Гамильтониана
    std::vector <Expression> O;//Переменная Наблюдаемой
    std::vector <Expression> O1;
    std::vector <int> I;
    std::ifstream file(argv[1], std::ifstream::in);//огткрфваем файл в котором содерижится наблюдаемая и гамильтониан
    std::string str;
    std::getline(file, str);
    if (str=="H:"){
        std::getline(file, str); 
    }
    else{
        assert(0);
    }
    while(str!="O:") {
        Expression t(str);//Создаем из строчки выражение для гамильтониана 
        H.push_back(t);//и помещаем в список
        std::getline(file, str);
    }
    //std::getline(file, str);
    if (str=="O:"){
        std::getline(file, str);
    }
    else{
        assert(0);
    }
    while(str!="n:") {
        Expression t(str);//Создаем из строчки выражение для наблюдаемой
        O.push_back(t);//и помещаем в список
        std::getline(file, str);
    }
    std::getline(file, str);
    if (str=="n:"){
        std::getline(file, str);
    }
    int n=0;
    for (int i = 0; i < (int)str.length(); i++) {
        n+=(str[i]-'0')*raiseToPow(10,(int)str.length()-i-1);//порядок разложения (сколько вложенных коммутаторов нужно посчитать)
    }
    file.close();
    int Nn=n;
std::cout << "Файл считан корректно"<< std::endl;
int nummm=0;
//#pragma omp parallel for schedule(dynamic)
//#pragma omp parallel for schedule(dynamic)
while(n!=0){
    directoryPath+=std::to_string(Nn-n+1);
    if (!std::filesystem::exists(directoryPath)) {
        std::filesystem::create_directory(directoryPath);
        std::cout << "Создана директория: " << Nn-n+1 << std::endl;
    } 
    if ((int)O.size()!=0){
        auto time2 = Clock::now();
        for (int i = 0; i < (int)H.size(); i++) {
            #pragma omp parallel for schedule(dynamic)
            for (int j = 0; j < (int)O.size(); j++) {
                H[i].Commute(O1,O[j]); //коммутируем все салгаемые с гамильтонанном.
            }
        }
        auto time3 = Clock::now();
        std::cout << n << "этап "<< (std::chrono::duration_cast<std::chrono::microseconds>(time3 - time2).count())/1000000.0 << " сек" << std::endl;
        //if (n==1){
        //        n=1;
        //for (int i= 0; i < (int)O1.size(); i++) {
        //    O1[i].print();
        //}
//}

//тут и дальше приведение подобных, сделано пока очень топорно, это самое узкое место
        for (int i = 0; i < (int)O1.size(); i++) {
            for (int j = i+1; j < (int)O1.size(); j++) {
                 if (O1[i]==O1[j]){
                    for (int l = 0; l < (int)O1[j].getPoly().size();l++) {
                        for (int k= 0; k < (int)O1[i].getPoly().size(); k++) {
                            if (*O1[i].getPoly()[k]==*O1[j].getPoly()[l]){
                                O1[i].getPoly()[k]->Sum(*O1[j].getPoly()[l]);
                                O1[j].PolyErase(l);
                                l--;
                                break;
                            }
                        }
                    }
                    for (int l = 0; l < (int)O1[j].getPoly().size(); l++) {
                        O1[i].PolyAdd(O1[j].getPoly()[l]);
                    }
                    O1.erase(O1.begin()+j);
                    j--;
                 }
            }
            bool b=false;
            for (int k= 0; k < (int)O1[i].getPoly().size(); k++) {
                if (O1[i].getPoly()[k]->isZero()==false){
                    b=true;
                    break;
                }
            }
            if (b==false){
                O1.erase(O1.begin()+i);
                i--;
            }
        }
        O.clear();
        n--;
        directoryPath.erase(directoryPath.size()-1);
        continue;
    }
    //после каждой итерации мы используем наблюдаемые из одного массива, например О и кладем резуцльтат в другой, например О1, тогда на следующей итерации в качестве наблюдаемой берем уже O1 а результат идет в O и т.д. После каждой итерации очищаем предыдущий список наблюдаемых
    if ((int)O1.size()!=0){ //проверка что это акуальная (не очищенная) наблюдаемая
        for (int i = 0; i < (int)H.size(); i++) {
            #pragma omp parallel for schedule(dynamic)
            for (int j = 0; j < (int)O1.size(); j++) {
                H[i].Commute(O,O1[j]); //коммутация с гамильтонаном
            }
        }
       // for (int i= 0; i < (int)O.size(); i++) {
       //     O[i].print();
       // }
       //приведение подобных
        for (int i = 0; i < (int)O.size(); i++) {
            for (int j = i+1; j < (int)O.size(); j++) {
                 if (O[i]==O[j]){
                    for (int l = 0; l < (int)O[j].getPoly().size();l++) {
                        for (int k= 0; k < (int)O[i].getPoly().size(); k++) {
                            if (*O[i].getPoly()[k]==*O[j].getPoly()[l]){
                                O[i].getPoly()[k]->Sum(*O[j].getPoly()[l]);
                                O[j].PolyErase(l);
                                l--;
                                break;
                            }
                        }
                    }
                    for (int l = 0; l < (int)O[j].getPoly().size(); l++) {
                        O[i].PolyAdd(O[j].getPoly()[l]);
                    }
                    O.erase(O.begin()+j);
                    j--;
                 }
            }
            bool b=false;
            for (int k= 0; k < (int)O[i].getPoly().size(); k++) {
                if (O[i].getPoly()[k]->isZero()==false){
                    b=true;
                    break;
                }
            }
            if (b==false){
                O.erase(O.begin()+i);
                i--;
            }
        }
        O1.clear();
        n--;
        directoryPath.erase(directoryPath.size()-1);
        continue;
    }
}
//вывод результата, пока он делается просто на экран
if ((int)O.size()!=0){
    for (int i= 0; i < (int)O.size(); i++) {
        O[i].print();
    }
}
else{
    for (int i= 0; i < (int)O1.size(); i++) {
        O1[i].print();
    }
}

auto time1 = Clock::now();
std::cout << "Вычисление заняло "<< (std::chrono::duration_cast<std::chrono::microseconds>(time1 - start_time).count())/1000000.0 << " сек" << std::endl;
//    getc(stdin);
//    getc(stdin);
        return 0;
}
