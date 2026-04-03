#include <string>
#include <iostream>
void dispatch_command(const std::string&); // defined elsewhere
void repl_read_and_dispatch(const std::string& prompt){
    std::string line;
    std::cout << prompt;
    while (std::getline(std::cin, line)){
        dispatch_command(line);
        std::cout << prompt;
    }
}



