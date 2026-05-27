#include <iostream>
#include <string>
#include <sstream>
#include <unordered_set>


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while(true) {
    std::cout << "$ ";
    std::string user_input;
    std::getline(std::cin, user_input);
    if(user_input == "exit") {
      return 0;
    }else if(user_input.substr(0,5) == "echo ") {
      for(int i = 5; i <= user_input.length(); i++) {
        if(user_input[i] == ' ') {
          continue;
        }
        std::string args = user_input.substr(i);
        std::cout << args<< std::endl;
        break;
      }
    }else if(user_input.substr(0,5) == "type ") {
      std::unordered_set<std::string> built_in_commands = {"echo", "type", "exit"};
      std::string args = user_input.substr(5);
      std::stringstream ss(args);
      std::string arg;
      while(ss >> arg) {
        if(built_in_commands.find(arg) != built_in_commands.end()) {
          std::cout << arg << " is a shell builtin" << std::endl;
        }else {
          std::cout << arg << ": not found" << std::endl;
        }
      }
    }
    else {
      std::cout << user_input << ": command not found" << std::endl;
    }
  }
  return 1;
}
