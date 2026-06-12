#include <cstdint>
#include <vector>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <string>
#include <iostream>
#include <cassert>
#include <cstdio>
#include "Expression.h"
#include <thread>
#include <mutex>
#include <filesystem>
#include <unordered_set>
#include <cmath>
#include <iomanip>
#include <sys/wait.h>
#include <sys/stat.h>
#include "Poly2.h"

struct Node {
    Expression expr;
    Node* next;

    Node(Expression&& expr_, Node* next_) : expr(std::move(expr_)) {
        next = next_;
    }
};

#define BLOCK_SIZE 2048

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

size_t getLogicalFileSize(const std::string& path) {
    struct stat stats;
    stat(path.c_str(), &stats);
    
    return stats.st_size;
}

size_t getRealFileSize(const std::string& path) {
    struct stat stats;
    stat(path.c_str(), &stats);
    
    return stats.st_blocks * 512;
}

std::pair<size_t, size_t> storeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream stream(path, std::ios::binary);
    stream.write((char*)(data.data()), data.size());
    stream.close();

    size_t realFileSize = getRealFileSize(path);
    return {data.size(), realFileSize};
}

std::vector<Expression> loadAndDeleteBlock(const std::string& path) {
    std::vector<uint8_t> data = loadFile(path);

    std::vector<Expression> res;
    size_t ptr = 0;
    while(ptr < data.size())
        res.emplace_back(data, ptr);

    return res;
}

std::pair<size_t, size_t> storeBlock(const std::string& path, const std::vector<Expression>& exprs) {
    std::vector<uint8_t> data;
    for(size_t i = 0; i < exprs.size(); i++)
        exprs[i].serialize(data);
    return storeFile(path, data);
}

struct Config {
    std::string directoryPath;
    size_t memoryLimitInBytes;
    bool overwriteMainDirectory;
    bool textOutput;
};
Config config;

std::string getPathToHashTableFile(size_t level, size_t start) {
    return config.directoryPath + "/" + std::to_string(level) + "/" + std::to_string(start) + ".comm";
}

struct DiskMovementResult {
    size_t numCommutators;
    size_t writtenDiskSize;
    size_t realDiskSize;
};
DiskMovementResult moveHashTableBlockToDisk(size_t level, size_t start, size_t tableSize, Node** indices, size_t tableOffset) {
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
    auto [writtenDiskSize, realDiskSize] = storeBlock(getPathToHashTableFile(level, start), exprs);
    return {exprs.size(), writtenDiskSize, realDiskSize};
}

DiskMovementResult moveHashTableToDisk(size_t level, Node** indices, size_t minHash, size_t maxHash) {
    DiskMovementResult res = {0, 0, 0};
    size_t nextHash = minHash;
    std::mutex taskMutex, resMutex;
    
    std::vector<std::thread> threads;
    size_t num_threads = std::thread::hardware_concurrency();
    for(size_t tt = 0; tt < num_threads; tt++) {
        threads.emplace_back([&nextHash, &res, &taskMutex, &resMutex, indices, level, minHash, maxHash]() {
            while(true) {
                std::unique_lock<std::mutex> lock(taskMutex);
                if(nextHash >= maxHash)
                    return;
                size_t curHash = nextHash;
                nextHash += BLOCK_SIZE;
                lock.unlock();

                auto [added, writtenDiskSize, realDiskSize] = moveHashTableBlockToDisk(level, curHash, maxHash, indices, minHash);

                std::lock_guard<std::mutex> lock2(resMutex);
                res.numCommutators += added;
                res.writtenDiskSize += writtenDiskSize;
                res.realDiskSize += realDiskSize;
            }
        });
    }
    for(size_t j = 0; j < threads.size(); j++)
        threads[j].join();
    
    return res;
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
    size_t writtenDiskSize;
    size_t realDiskSize;
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
                        }); // commute all terms of the expression with the Hamiltonian
            }
        });
    }
    for(size_t j = 0; j < threads.size(); j++)
        threads[j].join();
    
    size_t memoryUsageEnd = getMemoryUsage();
        
    delete[] indexMutexes;
    auto timeLayerEnd = Clock::now();
    std::cerr << "\e[90m" << "Collecting and combining like terms: "
              << std::chrono::duration_cast<std::chrono::microseconds>(timeLayerEnd - timeLayerStart).count() / 1000000.0 << " sec" << "\e[0m" << std::endl;
    
    // Save the new layer to disk and free it from memory
    auto [O_size, writtenDiskSize, realDiskSize] = moveHashTableToDisk(curLevelNum, indices, minHash, maxHash);
    delete[] indices;

    auto timeMoveEnd = Clock::now();
    std::cerr << "\e[90m" << "Saving to disk: "
              << std::chrono::duration_cast<std::chrono::microseconds>(timeMoveEnd - timeLayerEnd).count() / 1000000.0 << " sec" << "\e[0m" << std::endl;
    return {O_size, writtenDiskSize, realDiskSize, (memoryUsageEnd >= memoryUsageStart) ? (memoryUsageEnd - memoryUsageStart) : 0};
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
            std::cerr << "Failed to complete computation successfully" << std::endl;
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

std::string help_usage1 = "validate [FILE1] [FILE2]";
std::string help_usage2 = "momentum [DIRECTORY]";
std::string help_usage3 = "quench [DIRECTORY]";
std::string help_usage4 = "--help";
std::string help_usage5 = "--version";
std::string help_usage6 = "[FILE] [OPTION]...";
std::string help =
    "Perform actions on sums of Pauli strings.\n"
    "\n"
    "validate:\n"
    "    This sub-command accepts two files, both of which should contain Pauli\n"
    "    strings, one per line. The files should be in human-readable format.\n"
    "    The sums of strings from these files are compared; it is reported whether\n"
    "    or not the sums match.\n"
    "    Sometimes the exact mismatch reason will also be reported.\n"
    "\n"
    "momentum:\n"
    "    This sub-command accepts a path to a directory with Pauli string sums\n"
    "    The directory structure should follow the pattern below:\n"
    "\n"
    "        top_path\n"
    "        |  0\n"
    "        |  |  0.comm\n"
    "        |  |  512.comm\n"
    "        |  |  ...\n"
    "        |  1\n"
    "        |  |  0.comm\n"
    "        |  |  512.comm\n"
    "        |  |  ...\n"
    "\n"
    "    For each sub-directory (0 and 1 in the example above), the momentum\n"
    "    of the sum of all strings saved in that sub-directory will be\n"
    "    computed. Momentum is the sum of squares of all polynomials.\n"
    "\n"
    "quench:\n"
    "    This sub-command works exactly like \"momenum\", but computes quench\n"
    "    coefficients rather than momentums. To compute the quench coefficient,\n"
    "    for each Pauli string, the number of x, y, and z sigmas is computed,\n"
    "    and for each unique triple (nx, ny, nz) of counts, the sum of\n"
    "    polynomials of all Pauli strings with these counts is computed. The\n"
    "    resulted is returned as a polynomial with three new variables px, py and\n"
    "    pz representing sigma counts as their degrees.\n"
    "\n"
    "--help:\n"
    "    Print this message and exit.\n"
    "\n"
    "--version:\n"
    "    Print the version of the program and exit.\n"
    "\n"
    "otherwise:\n"
    "    Compute nested commutators for the problem described in FILE. The file\n"
    "    format is the following:\n"
    "\n"
    "        H:\n"
    "        <Pauli string>\n"
    "        ...\n"
    "        O:\n"
    "        <Pauli string>\n"
    "        ...\n"
    "        n:\n"
    "        <number of commutators to compute>\n"
    "\n"
    "    See the file examples/Comm.txt for a concrete example of a file, or visit\n"
    "    https://gitlab.com/viacheslav_hrushev/commutators/examples/Comm.txt\n"
    "\n"
    "    The result will be saved as a directory with a sub-directory for each\n"
    "    computed commutator (see the explanation of \"momentum\" sub-command\n"
    "    above).\n"
    "\n"
    "    Other options are listed below:\n"
    "\n"
    "    --memory-limit N     Sets the memory limit of N megabytes for the whole\n"
    "                         program. The default is no memory limit.\n"
    "    --res-dir DIRECTORY  Sets the path that the commutators will be written to\n"
    "                         to DIRECTORY. The default is ./tmp\n"
    "    --force              Delete the contents of the result directory before\n"
    "                         starting the computation. If the directory is not\n"
    "                         empty and this option is not set, an error will be\n"
    "                         thrown.\n"
    "    --text-output        Print the final computed commutator in human-readable\n"
    "                         format. Warning: for large commutators, this will be\n"
    "                         very slow.\n";
                      
int
main(int argc, char** argv)
{
    // Show help when the program is invoked with no arguments so that it does
    // not crash on the unconditional argv[1] access further below.
    if(argc <= 1) {
        std::cout << "Usage: " << argv[0] << " " << help_usage6 << std::endl;
        std::cout << "Run '" << argv[0] << " --help' for full usage information." << std::endl;
        return 1;
    }

    if(std::string(argv[1]) == "--version") {
        std::cout << "Commutators v1.0" << std::endl;
        std::cout << std::endl;
        std::cout << "Copyright (c) 2025 Ilya Shirokov, Viacheslav Khrushchev" << std::endl;
        std::cout << "Licensed under Creative Commons Attribution-NoDerivatives 4.0 International." << std::endl;
        std::cout << "See https://creativecommons.org/licenses/by-nd/4.0/ for the full license text." << std::endl;
        return 0;
    } else if(std::string(argv[1]) == "--help") {
        std::cout << "Usage: " << argv[0] << " " << help_usage1 << std::endl;
        std::cout << "   or: " << argv[0] << " " << help_usage2 << std::endl;
        std::cout << "   or: " << argv[0] << " " << help_usage3 << std::endl;
        std::cout << "   or: " << argv[0] << " " << help_usage4 << std::endl;
        std::cout << "   or: " << argv[0] << " " << help_usage5 << std::endl;
        std::cout << "   or: " << argv[0] << " " << help_usage6 << std::endl;
        std::cout << std::endl;
        std::cout << help;
        return 0;
    } else if(std::string(argv[1]) == "validate") {
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
            std::cout << "The first file contains duplicate terms, validation failed" << std::endl;
            return 0;
        }
        if(src.size() != new_.size()) {
            std::cout << "The files have a different number of terms" << std::endl;
            return 0;
        }

        for(size_t i = 0; i < new_.size(); i++) {
            auto it = remain_src.find(new_[i]);
            if(it == remain_src.end()) {
                std::cout << "Files do not match" << std::endl;
                return 0;
            }
            if(!(it->getPoly() == new_[i].getPoly())) {
                std::cout << "Files do not match" << std::endl;
                return 0;
            }
            remain_src.erase(it);
        }
        std::cout << "Files match" << std::endl;
        return 0;
    } else if(std::string(argv[1]) == "momentum" || std::string(argv[1]) == "quench") {
        if(argc <= 2) {
            std::cout << "Please specify the path to the computed commutators" << std::endl;
            return 1;
        }
        std::string directoryPath(argv[2]);

        auto mapToPolyAndSum = [&](const std::string& layerPath, std::function<Poly(const Expression& expr)> f) {
            auto it = std::filesystem::directory_iterator(layerPath);
            Poly2 res;
            std::vector<std::thread> threads;
            size_t num_threads = std::thread::hardware_concurrency();
            std::mutex taskMutex, resMutex;
            for(size_t tt = 0; tt < num_threads; tt++) {
                threads.emplace_back([&it, &res, &taskMutex, &resMutex, &f]() {
                    Poly2 resCur;
                    while(true) {
                        std::unique_lock<std::mutex> lock(taskMutex);
                        if(it == std::filesystem::directory_iterator{})
                            break;
                        std::string filepath = it->path();
                        it++;
                        lock.unlock();
                         
                        std::vector<Expression> expressions = loadAndDeleteBlock(filepath);
                        for(const Expression& expr : expressions)
                            resCur += Poly2(f(expr));
                    }
                    std::lock_guard<std::mutex> lock2(resMutex);
                    res += resCur;
                });
            }
            for(size_t j = 0; j < threads.size(); j++)
                threads[j].join();

            return res;
        };

        std::cout << ((std::string(argv[1]) == "momentum") ? "moments = {" : "quenchCoef = {") << std::endl;
        for(size_t i = 1; ; i++) {
            std::string layerPath = directoryPath + "/" + std::to_string(i);
            if(!std::filesystem::exists(layerPath))
                break;

            std::cerr << "\e[32m" << "Step " << i << "\e[0m" << std::endl;
            auto timeLayerStart = Clock::now();

            if(std::string(argv[1]) == "momentum") {
                Poly2 momentum = mapToPolyAndSum(layerPath, [](const Expression& expr) {
                    return expr.getPoly() * expr.getPoly();
                });
                if(i > 1)
                    std::cout << "," << std::endl;
                momentum.printAsMathematica(0, [](size_t ind) {
                    return "h" + std::to_string(ind);
                });
                std::cerr << "Commutator moment " << i << " computed" << std::endl;
            } else {
                Poly2 coef = mapToPolyAndSum(layerPath, [](const Expression& expr) {
                    auto [numSigmaX, numSigmaY, numSigmaZ] = expr.getNumSigmas();
                    Poly multiplyBy = Poly("[1," + std::to_string(numSigmaX) + "," + std::to_string(numSigmaY) + "," + std::to_string(numSigmaZ) + "]");
                    return expr.getPoly().prependVariables(3) * multiplyBy;
                });
                if(i > 1)
                    std::cout << "," << std::endl;
                coef.printAsMathematica(3, [](size_t ind) {
                    if(ind < 3)
                        return "p" + std::string(1, 'x' + ind);
                    return "h" + std::to_string(ind - 3);
                });
                std::cerr << "Quench coefficient of commutator " << i << " computed" << std::endl;
            }
            
            double timeOnCurLayer = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - timeLayerStart).count() / 1000000.0;
            std::cerr << "Total time on step: " << timeOnCurLayer << " sec" << std::endl;
            std::cerr << std::endl;
        }
        std::cout << std::endl;
        std::cout << "};" << std::endl;
        return 0;
    }

    // Parse command-line arguments
    config.directoryPath = "./tmp"; // directory where commutator files are stored 
    config.memoryLimitInBytes = std::numeric_limits<size_t>::max();
    config.overwriteMainDirectory = false;
    config.textOutput = false;

    for(int i = 2; i < argc;) {
        if(std::string(argv[i]) == "--memory-limit") {
            config.memoryLimitInBytes = std::stoull(argv[i + 1]) * 1'048'576;
            i += 2;
        } else if(std::string(argv[i]) == "--res-dir") {
            config.directoryPath = std::string(argv[i + 1]);
            i += 2;
        } else if(std::string(argv[i]) == "--force") {
            config.overwriteMainDirectory = true;
            i++;
        } else if(std::string(argv[i]) == "--text-output") {
            config.textOutput = true;
            i++;
        } else {
            std::cerr << "Unknown command-line argument: \"" << argv[i] << "\"" << std::endl;
            return 1;
        }
    }
    
    setlocale(LC_ALL, "");
    
    std::vector<Expression> H; // Hamiltonian
    std::vector<Expression> O_start; // Observable
    size_t O_size = 0;
    
    std::ifstream file(argv[1], std::ifstream::in); // open the file containing the observable and Hamiltonian
    std::string str;

    // Hamiltonian
    std::getline(file, str);
    assert(str == "H:");

    std::getline(file, str); 
    while(str != "O:") {
        H.emplace_back(str);
        std::getline(file, str);
    }

    // Observable
    assert(str == "O:");

    std::getline(file, str);
    while(str != "n:") {
        O_start.emplace_back(str);
        O_size++;
        std::getline(file, str);
    }

    // Number of commutators
    assert(str == "n:");
    std::getline(file, str);
    int n = std::stoi(str);
    
    file.close();
    std::cerr << "File parsed successfully" << std::endl;

    // Prepare the commutator output directory
    if(!std::filesystem::exists(config.directoryPath))
        std::filesystem::create_directory(config.directoryPath);
    if(!std::filesystem::is_empty(config.directoryPath)) {
        if(!config.overwriteMainDirectory) {
            std::cerr << "Error: directory " + config.directoryPath + " is not empty" << std::endl;
            std::cerr << "Add the --force option to overwrite its contents, or specify an empty directory" << std::endl;
            return 1;
        } else {
            std::cerr << "--force specified, clearing non-empty directory " + config.directoryPath + "..." << std::endl;

            std::vector<std::filesystem::path> toDelete;
            for(auto it = std::filesystem::directory_iterator(config.directoryPath); it != std::filesystem::directory_iterator{}; it++)
                toDelete.push_back(*it);
            for(const std::filesystem::path& path : toDelete)
                std::filesystem::remove_all(path);
        }
    }
    std::cerr << std::endl;

    // Save layer 0 (the initial observable O) to disk
    if(!std::filesystem::exists(config.directoryPath + "/0"))
        std::filesystem::create_directory(config.directoryPath + "/0");
    size_t prevTableSize = O_start.size();
    size_t prevWrittenDiskSize = 0;
    size_t prevRealDiskSize = 0;
    for(size_t i = 0; i < O_start.size(); i += BLOCK_SIZE) {
        std::vector<Expression> cur(O_start.begin() + i, O_start.begin() + std::min(O_start.size(), i + BLOCK_SIZE));
        std::sort(cur.begin(), cur.end());
        auto [writtenDiskSize, realDiskSize] = storeBlock(getPathToHashTableFile(0, i), cur);
        prevWrittenDiskSize += writtenDiskSize;
        prevRealDiskSize += realDiskSize;
    }

    auto timeStart = Clock::now(); // clock for measuring execution time
    double prevExpansionRatio = H.size();
    double averageBytesPerHash = sizeof(Node) + sizeof(Node*);
    double timeOnPrevLayer = 0;
    for(int32_t t = 0; t < n; t++) {
        std::cerr << "\e[32m" << "Step " << t + 1 << "\e[0m" << std::endl;
        if(!std::filesystem::exists(config.directoryPath + "/" + std::to_string(t + 1))) {
            std::filesystem::create_directory(config.directoryPath + "/" + std::to_string(t + 1));
            std::cerr << "Created directory: " << t + 1 << std::endl;
        }

        auto timeLayerStart = Clock::now();
        size_t tableSize = std::max((size_t)1, (size_t)std::ceil(prevExpansionRatio * O_size));
#ifdef DEBUG
        std::cerr << "[DIAG] layer=" << t+1
                  << " O_size=" << O_size
                  << " prevExpRatio=" << std::fixed << std::setprecision(6) << prevExpansionRatio
                  << " tableSize=" << tableSize
                  << " avgBytesPerHash=" << std::setprecision(2) << averageBytesPerHash
                  << std::endl;
#endif

        auto safeSubtract = [](size_t x, size_t y) {
            return (x >= y) ? (x - y) : 0;
        };
        static_assert(sizeof(Node) >= sizeof(Expression));
        size_t memoryUsagePerOldExpression = safeSubtract(averageBytesPerHash, (sizeof(Node) - sizeof(Expression)) + sizeof(std::mutex) / 16);
        size_t fixedMemoryUsage = std::thread::hardware_concurrency() * (BLOCK_SIZE * memoryUsagePerOldExpression + 131'072);
        
        size_t cur_O_size = 0;
        size_t curWrittenDiskSize = 0;
        size_t curRealDiskSize = 0;
        size_t sumOfMemoryUsages = 0;
        // commute all terms of O with the Hamiltonian
        size_t windowIndex = 0;
        for(size_t i = 0; i < tableSize;) {
            size_t maxNumHashes = safeSubtract(config.memoryLimitInBytes, fixedMemoryUsage) / averageBytesPerHash;
            size_t numHashes = std::min(tableSize - i, std::max((size_t)BLOCK_SIZE, maxNumHashes - maxNumHashes % BLOCK_SIZE));

#ifdef DEBUG
            std::cerr << "[DIAG]   window=" << windowIndex
                      << " slots=[" << i << ", " << i + numHashes << ")"
                      << " maxNumHashes=" << maxNumHashes
                      << std::endl;
#endif

            ComputationResult res = computePartOfNextLayer(H, prevTableSize, t + 1, tableSize, i, i + numHashes);

            cur_O_size += res.O_size;
            curWrittenDiskSize += res.writtenDiskSize;
            curRealDiskSize += res.realDiskSize;
            sumOfMemoryUsages += res.maxMemoryUsed;
            averageBytesPerHash = std::max((double)sumOfMemoryUsages / (i + numHashes), (double)(sizeof(Node) + sizeof(Node*)));

#ifdef DEBUG
            std::cerr << "[DIAG]   window=" << windowIndex
                      << " O_size_so_far=" << cur_O_size
                      << " rawMemUsed=" << res.maxMemoryUsed / 1'000'000 << " MB"
                      << " avgBytesPerHash=" << std::setprecision(2) << averageBytesPerHash
                      << std::endl;
#endif

            i += numHashes;
            windowIndex++;
        }
        curRealDiskSize += getLogicalFileSize(config.directoryPath + "/" + std::to_string(t + 1));

        prevExpansionRatio = (double)cur_O_size / O_size;
        O_size = cur_O_size;
        prevTableSize = tableSize;

        auto timeLayerFinish = Clock::now();
        std::cerr << "Layer " << t + 1 << " computed; total terms: " << O_size << " (x" << prevExpansionRatio << " from previous layer)" << std::endl;
        std::cerr << "Written to disk: ~" << curWrittenDiskSize / 1'000'000 << " MB (x" << (double)curWrittenDiskSize / prevWrittenDiskSize << " from previous layer)" << std::endl;
        std::cerr << "Actual disk usage: ~" << curRealDiskSize / 1'000'000 << " MB (x" << (double)curRealDiskSize / prevRealDiskSize << " from previous layer)" << std::endl;
        double timeOnCurLayer = std::chrono::duration_cast<std::chrono::microseconds>(timeLayerFinish - timeLayerStart).count() / 1000000.0;
        std::cerr << "Total time on layer: " << timeOnCurLayer << " sec";
        if(t > 0)
            std::cerr << " (x" << timeOnCurLayer / timeOnPrevLayer << " from previous layer)" << std::endl;
        timeOnPrevLayer = timeOnCurLayer;
        prevWrittenDiskSize = curWrittenDiskSize;
        prevRealDiskSize = curRealDiskSize;
        std::cerr << std::endl;
    }

    auto timeEnd = Clock::now();
    std::cerr << "Computation took " << std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart).count() / 1000000.0 << " sec" << std::endl;
    
    if(config.textOutput) {
        for(size_t i = 0; i < prevTableSize; i += BLOCK_SIZE)
            for(const auto& expr : loadAndDeleteBlock(getPathToHashTableFile(n, i)))
                expr.print();
        
        auto timeFinish = Clock::now();
        std::cerr << "Text output took " << std::chrono::duration_cast<std::chrono::microseconds>(timeFinish - timeEnd).count() / 1000000.0 << " sec" << std::endl;
    }

    return 0;
}
