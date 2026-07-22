#include "uci.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv){
    int threads = 1;
    for(int index = 1; index < argc; index++){
        const std::string argument = argv[index];
        if(argument == "--help" || argument == "-h"){
            std::cout << "Usage: tiramisu-uci [--uci] [--threads N]\n";
            return 0;
        }
        if(argument == "--uci") continue; // Compatibility with tournament scripts.
        if(argument == "--threads" && index + 1 < argc){
            try{
                threads = std::stoi(argv[++index]);
            } catch(...){
                std::cerr << "Invalid --threads value\n";
                return 1;
            }
            const int maximum = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
            if(threads < 1 || threads > maximum){
                std::cerr << "--threads must be between 1 and " << maximum << "\n";
                return 1;
            }
            continue;
        }

        std::cerr << "Unknown argument: " << argument << "\n";
        return 1;
    }

    return runUCILoop(threads);
}
