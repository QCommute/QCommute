#include <cstdint>
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
#include <unordered_map>
#include <set>
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
    std::cout << "Файл считан корректно"<< std::endl;

//#pragma omp parallel for schedule(dynamic)
//#pragma omp parallel for schedule(dynamic)
for(int32_t t = 0; t < n; t++) {
    std::cout << "Этап " << t + 1 << std::endl;
    directoryPath+=std::to_string(t + 1);
    if (!std::filesystem::exists(directoryPath)) {
        std::filesystem::create_directory(directoryPath);
        std::cout << "Создана директория: " << t + 1 << std::endl;
    } 

        auto time2 = Clock::now();
        for (int i = 0; i < (int)H.size(); i++) {
            #pragma omp parallel for schedule(dynamic)
            for (int j = 0; j < (int)O.size(); j++) {
                H[i].Commute(O1,O[j]); //коммутируем все салгаемые с гамильтонанном.
            }
        }
        auto time3 = Clock::now();
        std::cout << "Подсчет коммутаторов: " << (std::chrono::duration_cast<std::chrono::microseconds>(time3 - time2).count())/1000000.0 << " сек" << std::endl;

//тут и дальше приведение подобных, сделано пока очень топорно, это самое узкое место
        auto time4 = Clock::now();
        std::unordered_map<Expression, int32_t, ExpressionHasher> indices;
        //std::set<size_t> hashSet;
        std::vector<bool> good(O1.size(), true);
        for(int32_t j = 0; j < (int32_t)O1.size(); j++) {
            //hashSet.insert(ExpressionHasher()(O1[j]));
            auto it = indices.find(O1[j]);
            if(it == indices.end()) {
                indices[O1[j]] = j;
            } else {
                int32_t i = it->second;
                O1[i].getPoly() += O1[j].getPoly();

                good[j] = false;
            }
        }
        //std::cout << "Total " << O1.size() << "; unique " << indices.size() << "; unique hashes " << hashSet.size() << "\n";
    
        for (int i = 0; i < (int)O1.size(); i++) {
            if(!good[i])
                continue;
            good[i] = good[i] && (!O1[i].getPoly().isZero());
        }
    
        O.clear();
        for(int32_t i = 0; i < O1.size(); i++)
            if(good[i])
                O.push_back(O1[i]);
        O1.clear();
    
        directoryPath.erase(directoryPath.size()-1);
        auto time5 = Clock::now();
        std::cout << "Приведение подобных: " << (std::chrono::duration_cast<std::chrono::microseconds>(time5 - time4).count())/1000000.0 << " сек" << std::endl;
        std::cout << std::endl;
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
