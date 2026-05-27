#include <iostream>
#include <string>
#include <boost/algorithm/string.hpp>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while(true) {
    std::cout << "$ ";
    std::string input;
    std::getline(std::cin, input);
    if(input == "exit") {
      return 0;
    }else if(input.subtr(0,5) == "echo ") {
      std::string rest = input.substring(5);
      std::cout << boost::trim(rest) << std::endl;
    }else {
      std::cout << input << ": command not found" << std::endl;
    }
  }
  return 1;
}
