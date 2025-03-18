//#include <bits/fs_dir.h>
#include <cstdint>
#include <vector>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <string>
#include <iostream>
#include <cassert>
#include <ctime>
#include <cstdio>
#include "Expression.h"
#include <thread>
#include <mutex>
#include <filesystem>
#include <unordered_set>
#include <sys/wait.h>

struct Node {
    Expression expr;
    Node* next;

    Node(Expression&& expr_, Node* next_) : expr(std::move(expr_)) {
        next = next_;
    }
};

#define BLOCK_SIZE 512

std::vector<uint8_t> loadFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    stream.seekg(0, std::ios::end);
    size_t size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> data;
    data.resize(size);
    stream.read((char*)(data.data()), size);
    return data;
}

void storeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream stream(path, std::ios::binary);
    stream.write((char*)(data.data()), data.size());
}

std::vector<Expression> loadAndDeleteBlock(const std::string& path) {
    std::vector<uint8_t> data = loadFile(path);

    std::vector<Expression> res;
    size_t ptr = 0;
    while(ptr < data.size())
        res.emplace_back(data, ptr);

    return res;
}

void storeBlock(const std::string& path, const std::vector<Expression>& exprs) {
    std::vector<uint8_t> data;
    for(size_t i = 0; i < exprs.size(); i++)
        exprs[i].serialize(data);
    storeFile(path, data);
}

struct Config {
    std::string directoryPath;
    size_t memoryLimitInBytes;
};
Config config;

std::string getPathToHashTableFile(size_t level, size_t start) {
    return config.directoryPath + "/" + std::to_string(level) + "/" + std::to_string(start) + ".comm";
}

size_t moveHashTableBlockToDisk(size_t level, size_t start, size_t tableSize, Node** indices, size_t tableOffset) {
    std::vector<Expression> exprs;
    for(size_t i = start; i < std::min(start + BLOCK_SIZE, tableSize); i++) {
        Node* cur_node = indices[i - tableOffset];
        while(cur_node != nullptr) {
            Node* next_node = cur_node->next;
            if(!cur_node->expr.getPoly().isZero())
                exprs.push_back(std::move(cur_node->expr));
            delete cur_node;
            cur_node = next_node;
        }
        indices[i - tableOffset] = nullptr;
    }
    storeBlock(getPathToHashTableFile(level, start), exprs);
    return exprs.size();
}

size_t moveHashTableToDisk(size_t level, Node** indices, size_t minHash, size_t maxHash) {
    size_t O_size = 0;
    size_t nextHash = minHash;
    std::mutex taskMutex, resMutex;
    
    std::vector<std::thread> threads;
    size_t num_threads = std::thread::hardware_concurrency();
    for(size_t tt = 0; tt < num_threads; tt++) {
        threads.emplace_back([&nextHash, &O_size, &taskMutex, &resMutex, indices, level, minHash, maxHash]() {
            while(true) {
                std::unique_lock<std::mutex> lock(taskMutex);
                if(nextHash >= maxHash)
                    return;
                size_t curHash = nextHash;
                nextHash += BLOCK_SIZE;
                lock.unlock();

                size_t added = moveHashTableBlockToDisk(level, curHash, maxHash, indices, minHash);

                std::lock_guard<std::mutex> lock2(resMutex);
                O_size += added;
            }
        });
    }
    for(size_t j = 0; j < threads.size(); j++)
        threads[j].join();
    
    return O_size;
}

size_t getMemoryUsage() {
    std::ifstream stream("/proc/self/status");
    std::string str;
    while(stream >> str) {
        if(str == "VmRSS:") {
            std::string value;
            stream >> value;
            
            std::string suffix;
            stream >> suffix;
            assert(suffix == "kB");

            return std::stoll(value) * 1024;
        }
    }
    return 0;
}

using Clock = std::chrono::high_resolution_clock;

struct ComputationResult {
    size_t O_size;
    size_t maxMemoryUsed;
};

ComputationResult computePartOfNextLayerImpl(const std::vector<Expression>& H, size_t prevTableSize,
                                             size_t curLevelNum, size_t curTableSize,
                                             size_t minHash, size_t maxHash) {
    assert(minHash % BLOCK_SIZE == 0);
    assert(maxHash >= curTableSize || maxHash % BLOCK_SIZE == 0);
    assert(maxHash > minHash);

    size_t memoryUsageStart = getMemoryUsage();
    
    auto timeLayerStart = Clock::now();
    Node** indices = new Node*[maxHash - minHash];
    for(size_t i = 0; i < maxHash - minHash; i++)
        indices[i] = nullptr;
    std::mutex* indexMutexes = new std::mutex[(maxHash - minHash + 15) / 16];
    
    size_t nextHash = 0;
    std::mutex taskMutex;
    
    std::vector<std::thread> threads;
    size_t num_threads = std::thread::hardware_concurrency();
    for(size_t tt = 0; tt < num_threads; tt++) {
        threads.emplace_back([&nextHash, prevTableSize, &taskMutex, indices, curTableSize, indexMutexes, &H, curLevelNum, minHash, maxHash] () {
            while(true) {
                std::unique_lock<std::mutex> lock(taskMutex);
                if(nextHash >= prevTableSize)
                    return;
                size_t curHash = nextHash;
                nextHash += BLOCK_SIZE;
                lock.unlock();
                
                std::vector<Expression> expressionsCur = loadAndDeleteBlock(getPathToHashTableFile(curLevelNum - 1, curHash));
                
                for (size_t i = 0; i < H.size(); i++)
                    for(size_t j = 0; j < expressionsCur.size(); j++)
                        H[i].Commute(expressionsCur[j], [&](Expression&& newExpression) {
                            size_t hash = ExpressionHasher()(newExpression);
                            if(hash  % curTableSize < minHash || hash  % curTableSize >= maxHash)
                                return;
                            std::lock_guard<std::mutex> guard(indexMutexes[(hash % curTableSize - minHash) / 16]);
                            Node* prev_node = nullptr;
                            Node* cur_node = indices[hash % curTableSize - minHash];
                            bool found = false;
                            while(cur_node != nullptr) {
                                if(cur_node->expr == newExpression) {
                                    cur_node->expr.getPoly() += newExpression.getPoly();
                                    found = true;
                                    break;
                                } else if(newExpression < cur_node->expr)
                                    break;
                                prev_node = cur_node;
                                cur_node = cur_node->next;
                            }
                            if(!found) {
                                if(prev_node == nullptr)
                                    indices[hash % curTableSize - minHash] = new Node(std::move(newExpression), indices[hash % curTableSize - minHash]);
                                else
                                    prev_node->next = new Node(std::move(newExpression), cur_node);
                            }
                        }); //коммутируем все салгаемые с гамильтонанном.
            }
        });
    }
    for(size_t j = 0; j < threads.size(); j++)
        threads[j].join();
    
    size_t memoryUsageEnd = getMemoryUsage();
        
    delete[] indexMutexes;
    auto timeLayerEnd = Clock::now();
    std::cerr << "\e[90m" << "Подсчет слагаемых и приведение подобных: "
              << std::chrono::duration_cast<std::chrono::microseconds>(timeLayerEnd - timeLayerStart).count() / 1000000.0 << " сек" << "\e[0m" << std::endl;
    
    // Сохраняем новый слой на диск и удаляем его из памяти
    size_t O_size = moveHashTableToDisk(curLevelNum, indices, minHash, maxHash);
    delete[] indices;

    auto timeMoveEnd = Clock::now();
    std::cerr << "\e[90m" << "Сохранение на диск: "
              << std::chrono::duration_cast<std::chrono::microseconds>(timeMoveEnd - timeLayerEnd).count() / 1000000.0 << " сек" << "\e[0m" << std::endl;
    return {O_size, (memoryUsageEnd >= memoryUsageStart) ? (memoryUsageEnd - memoryUsageStart) : 0};
}

ComputationResult computePartOfNextLayer(const std::vector<Expression>& H, size_t prevTableSize,
                                         size_t curLevelNum, size_t curTableSize,
                                         size_t minHash = 0, size_t maxHash = std::numeric_limits<size_t>::max()) {
    int pipefd[2];
    pipe2(pipefd, 0);
    
    int fork_res = fork();
    if(fork_res == 0) {
        close(pipefd[0]);

        ComputationResult res = computePartOfNextLayerImpl(H, prevTableSize, curLevelNum, curTableSize, minHash, maxHash);

        ssize_t written = 0;
        while(written < sizeof(ComputationResult))
            written += write(pipefd[1], (char*)(&res) + written, sizeof(ComputationResult) - written);
        exit(0);
    } else {
        close(pipefd[1]);

        int wstatus;
        waitpid(fork_res, &wstatus, 0);
        if(!(WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0)) {
            std::cerr << "Не удалось корректно завершить вычисления" << std::endl;
            exit(1);
        }

        ComputationResult res;
        ssize_t read_ = 0;
        while(read_ < sizeof(ComputationResult))
            read_ += read(pipefd[0], (char*)(&res) + read_, sizeof(ComputationResult) - read_);
        close(pipefd[0]);
        return res;
    }
}

int
main(int argc, char** argv)
{
    if(std::string(argv[1]) == "validate") {
        std::vector<Expression> src, new_;
        std::string str;
        
        std::ifstream file_src(argv[2], std::ifstream::in);
        while(std::getline(file_src, str))
            src.push_back(Expression(str));

        std::ifstream file_new(argv[3], std::ifstream::in);
        while(std::getline(file_new, str))
            new_.push_back(Expression(str));

        std::unordered_set<Expression, ExpressionHasher> remain_src;
        for(size_t i = 0; i < src.size(); i++)
            remain_src.insert(src[i]);
        if(remain_src.size() != src.size()) {
            std::cout << "В первом файле есть подобные слагаемые, проверка не удалась" << std::endl;
            return 0;
        }
        if(src.size() != new_.size()) {
            std::cout << "В файлах разное количество слагаемых" << std::endl;
            return 0;
        }

        for(size_t i = 0; i < new_.size(); i++) {
            auto it = remain_src.find(new_[i]);
            if(it == remain_src.end()) {
                std::cout << "Файлы не совпадают" << std::endl;
                return 0;
            }
            if(!(it->getPoly() == new_[i].getPoly())) {
                std::cout << "Файлы не совпадают" << std::endl;
                return 0;
            }
            remain_src.erase(it);
        }
        std::cout << "Файлы совпадают" << std::endl;
        return 0;
    } else if(std::string(argv[1]) == "momentum") {
        if(argc <= 2) {
            std::cout << "Необходимо указать путь к посчитанным коммутаторам" << std::endl;
            return 1;
        }
        std::string directoryPath(argv[2]);

        for(size_t i = 0; ; i++) {
            std::string layerPath = directoryPath + "/" + std::to_string(i);
            if(!std::filesystem::exists(layerPath))
                return 0;

            std::cerr << "\e[32m" << "Этап " << i << "\e[0m" << std::endl;
            auto timeLayerStart = Clock::now();

            auto it = std::filesystem::directory_iterator(layerPath);
            Poly momentum;
            std::vector<std::thread> threads;
            size_t num_threads = std::thread::hardware_concurrency();
            std::mutex taskMutex, resMutex;
            for(size_t tt = 0; tt < num_threads; tt++) {
                threads.emplace_back([&it, &momentum, &taskMutex, &resMutex]() {
                    Poly momentumCur;
                    while(true) {
                        std::unique_lock<std::mutex> lock(taskMutex);
                        if(it == std::filesystem::directory_iterator{})
                            break;
                        std::string filepath = it->path();
                        it++;
                        lock.unlock();
                         
                        std::vector<Expression> expressions = loadAndDeleteBlock(filepath);
                        for(const Expression& expr : expressions)
                            momentumCur += expr.getPoly() * expr.getPoly();
                    }
                    std::lock_guard<std::mutex> lock2(resMutex);
                    momentum += momentumCur;
                });
            }
            for(size_t j = 0; j < threads.size(); j++)
                threads[j].join();

            momentum.print();
            std::cout << std::endl;
            
            std::cerr << "Момент коммутатора " << i << " посчитан" << std::endl;
            double timeOnCurLayer = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - timeLayerStart).count() / 1000000.0;
            std::cerr << "Суммарное время на слое: " << timeOnCurLayer << " сек" << std::endl;
            std::cerr << std::endl;
        }
        return 0;
    }

    // Разбираем агрументы командной строки
    config.directoryPath = "./tmp"; //место где хранится файлы с коммутаторами 
    config.memoryLimitInBytes = std::numeric_limits<size_t>::max();

    for(int i = 2; i < argc;) {
        if(std::string(argv[i]) == "--memory-limit") {
            config.memoryLimitInBytes = std::stoull(argv[i + 1]) * 1'048'576;
            i += 2;
        } else {
            std::cerr << "Неизвестный аргумент командной строки: \"" << argv[i] << "\"" << std::endl;
            return 1;
        }
    }
    
    auto timeStart = Clock::now();//просто часы чтобы мерить время исполнения
    setlocale(LC_ALL, "");
    
    std::vector<Expression> H; //Переменная Гамильтониана
    std::vector<Expression> O_start; //Переменная Наблюдаемой
    size_t O_size = 0;
    
    std::ifstream file(argv[1], std::ifstream::in); //Открываем файл в котором содерижится наблюдаемая и гамильтониан
    std::string str;

    // Гамильтониан
    std::getline(file, str);
    assert(str == "H:");

    std::getline(file, str); 
    while(str != "O:") {
        H.emplace_back(str);
        std::getline(file, str);
    }

    // Наблюдаемая
    assert(str == "O:");

    std::getline(file, str);
    while(str != "n:") {
        O_start.emplace_back(str);
        O_size++;
        std::getline(file, str);
    }

    // Число коммутаторов
    assert(str == "n:");
    std::getline(file, str);
    int n = std::stoi(str);
    
    file.close();
    std::cerr << "Файл считан корректно"<< std::endl;

    // Сохраняем нулевой слой (O из входного файла) на диск
    if(!std::filesystem::exists(config.directoryPath + "/0"))
        std::filesystem::create_directory(config.directoryPath + "/0");
    size_t prevTableSize = O_start.size();
    for(size_t i = 0; i < O_start.size(); i += BLOCK_SIZE) {
        std::vector<Expression> cur(O_start.begin() + i, O_start.begin() + std::min(O_start.size(), i + BLOCK_SIZE));
        std::sort(cur.begin(), cur.end());
        storeBlock(getPathToHashTableFile(0, i), cur);
    }

    double prevExpansionRatio = H.size();
    double averageBytesPerHash = sizeof(Node) + sizeof(Node*);
    double timeOnPrevLayer = 0;
    for(int32_t t = 0; t < n; t++) {
        std::cerr << "\e[32m" << "Этап " << t + 1 << "\e[0m" << std::endl;
        if(!std::filesystem::exists(config.directoryPath + "/" + std::to_string(t + 1))) {
            std::filesystem::create_directory(config.directoryPath + "/" + std::to_string(t + 1));
            std::cerr << "Создана директория: " << t + 1 << std::endl;
        }

        auto timeLayerStart = Clock::now();
        size_t tableSize = std::max((size_t)1, (size_t)std::ceil(prevExpansionRatio * O_size));

        size_t cur_O_size = 0;
        size_t sumOfMemoryUsages = 0;
        for(size_t i = 0; i < tableSize;) {
            size_t maxNumHashes = config.memoryLimitInBytes / averageBytesPerHash;
            size_t numHashes = std::min(tableSize - i, std::max((size_t)BLOCK_SIZE, maxNumHashes - maxNumHashes % BLOCK_SIZE));

            ComputationResult res = computePartOfNextLayer(H, prevTableSize, t + 1, tableSize, i, i + numHashes);

            cur_O_size += res.O_size;
            sumOfMemoryUsages += res.maxMemoryUsed;
            averageBytesPerHash = std::max((double)sumOfMemoryUsages / (i + numHashes), (double)(sizeof(Node) + sizeof(Node*)));
            i += numHashes;
        }

        prevExpansionRatio = (double)cur_O_size / O_size;
        O_size = cur_O_size;
        prevTableSize = tableSize;

        auto timeLayerFinish = Clock::now();
        std::cerr << "Слой " << t + 1 << " посчитан; всего слагаемых: " << O_size << " (x" << prevExpansionRatio << " с предыдущего слоя)" << std::endl;
        double timeOnCurLayer = std::chrono::duration_cast<std::chrono::microseconds>(timeLayerFinish - timeLayerStart).count() / 1000000.0;
        std::cerr << "Суммарное время на слое: " << timeOnCurLayer << " сек";
        if(t > 0)
            std::cerr << " (x" << timeOnCurLayer / timeOnPrevLayer << " с предыдущего слоя)" << std::endl;
        timeOnPrevLayer = timeOnCurLayer;
        std::cerr << std::endl;
    }

    auto timeEnd = Clock::now();
    std::cerr << "Вычисление заняло " << std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart).count() / 1000000.0 << " сек" << std::endl;
    
    //вывод результата, пока он делается просто на экран
    for(size_t i = 0; i < prevTableSize; i += BLOCK_SIZE)
        for(const auto& expr : loadAndDeleteBlock(getPathToHashTableFile(n, i)))
            expr.print();

    auto timeFinish = Clock::now();
    std::cerr << "Вывод занял " << std::chrono::duration_cast<std::chrono::microseconds>(timeFinish - timeEnd).count() / 1000000.0 << " сек" << std::endl;
    return 0;
}
