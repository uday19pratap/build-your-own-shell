#include <iostream>
#include <string>
#include <sstream>
#include <unordered_set>
#include <unistd.h>

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(true) {
    std::cout << "$ ";
    std::string user_input;
    std::getline(std::cin, user_input);
    if(user_input == "exit") {
      return 0;
    }else if(user_input.substr(0,5) == "echo ") {
      std::string args = user_input.substr(5);
      std::stringstream ss(args);
      std::string arg;
      while(ss >> arg) {
        std::cout << arg << " ";
      }
      std::cout << std::endl;
    }else if(user_input.substr(0,5) == "type ") {
      std::unordered_set<std::string> built_in_commands = {"echo", "type", "exit"};
      std::string args = user_input.substr(5);
      std::stringstream ss(args);
      std::string arg;
      while(ss >> arg) {
        //built in command
        if(built_in_commands.find(arg) != built_in_commands.end()) {
          std::cout << arg << " is a shell builtin" << std::endl;
        }else {
          //not a built in command, search for binary in PATH
          char* path_env = std::getenv("PATH");
          if(path_env != nullptr) { 
            std::stringstream path_ss(path_env);
            std::string path;
            while(std::getline(path_ss, path, PATH_LIST_SEPARATOR)) {
              std::string exec_path = path + "/" + arg;
              // 0 means file exists and has exec permission
              // -1 means no file/no exec permission
              if(access(exec_path.c_str(), X_OK) == 0) {
                std::cout << arg << " is " << exec_path << std::endl;
                break;
              }
            }
          }
          //neither a built in command nor a executable in PATH. Print not found.
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
