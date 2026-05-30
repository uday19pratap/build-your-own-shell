#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <unistd.h>
#include <vector>
#include<fcntl.h>

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif
const std::unordered_set<std::string> built_in_commands = {"echo", "type", "exit"};

struct ParsedCommand {
  std::string command;
  std::vector<std::string> args;
};

std::unordered_set<char> special_characters_in_double_quotes_set = {'\\', '$', '`', '\n', '"'};

std::vector<std::string> pre_process_input(const std::string& input) {

  std::vector<std::string> processed_args;
  std::string cur;

  enum State {NORMAL, IN_SINGLE, IN_DOUBLE};
  State state = NORMAL;

  // at a time we can only be inside one. if inside single quote double quotes are regular chars
  //and vice versa
  bool is_escape_char_activated = false;
  for(char ch : input) {

    //prev character was \ so this one will 
    if(is_escape_char_activated) {
      //only escape certain special characters
      // like " \ $ ` and newline
      if(state == IN_DOUBLE && !special_characters_in_double_quotes_set.contains(ch)) {
        cur += '\\';
      }
      cur += ch;
      is_escape_char_activated = false;
      continue;
    }
    //right now ' and " are same.. only difference is
    //inside ' , " has no special meaning and
    //inside " , ' has no special meaning

    //only one of ' / " is valid. Cant be inside both at at a time
    if(ch == '\'' && state != IN_DOUBLE) {
      state = (state == IN_SINGLE) ? NORMAL : IN_SINGLE;
      continue;
    }
    if(ch == '"' && state != IN_SINGLE) {
      state = (state == IN_DOUBLE) ? NORMAL : IN_DOUBLE;
      continue;
    }

    if(state == IN_SINGLE || state == IN_DOUBLE) {
      if(ch == '\\' && state == IN_DOUBLE) {
        is_escape_char_activated = true;
        continue;
      }
      cur += ch;
    }else {
      if(ch == '\\') {
        is_escape_char_activated = true;
        continue;
      }
      if(ch == ' ' && !cur.empty()) {
        processed_args.push_back(cur);
        cur.clear();
      }else if(ch == ' ') {
        continue;
      }else {
        cur += ch;
      }
    }
  }

  if(!cur.empty()) {
    processed_args.push_back(cur);
  }
  return processed_args;
}

ParsedCommand parse_command(const std::string& user_input) {

  ParsedCommand parsed;

  std::vector<std::string> tokens = pre_process_input(user_input);
  if(tokens.size() > 0) {
   parsed.command = tokens[0];
  }
  for(int i = 1; i < tokens.size(); i++) {
    parsed.args.push_back(tokens[i]);
  }
  return parsed;
}

using CommandHandler = std::function<void(const ParsedCommand&)>;

std::string find_executable_path(const std::string& command) {
  char* path_env = std::getenv("PATH");
  if(path_env == nullptr) {
    return "";
  }

  std::stringstream path_ss(path_env);
  std::string path;
  while(std::getline(path_ss, path, PATH_LIST_SEPARATOR)) {
    std::string exec_path = path + "/" + command;
    if(access(exec_path.c_str(), X_OK) == 0) {
      return exec_path;
    }
  }

  return "";
}

void handle_echo(const ParsedCommand& parsed_command) {
  for(const std::string& arg : parsed_command.args) {
    std::cout << arg << " ";
  }
  std::cout << std::endl;
}

void handle_type(const ParsedCommand& parsed_command) {
  for(const std::string& arg : parsed_command.args) {
    bool bFoundArg = false;
    if(built_in_commands.find(arg) != built_in_commands.end()) {
      std::cout << arg << " is a shell builtin" << std::endl;
      bFoundArg = true;
    } else {
      std::string exec_path = find_executable_path(arg);
      if(!exec_path.empty()) {
        std::cout << arg << " is " << exec_path << std::endl;
        bFoundArg = true;
      }
    }

    if(!bFoundArg) {
      std::cout << arg << ": not found" << std::endl;
    }
  }
}

void handle_external(const ParsedCommand& parsed_command, const std::string& user_input) {
  std::string exec_path = find_executable_path(parsed_command.command);
  if(exec_path.empty()) {
    std::cout << parsed_command.command << ": command not found" << std::endl;
    return;
  }

  std::system(user_input.c_str());
}

const std::unordered_map<std::string, CommandHandler> built_in_handlers = {
  {"echo", handle_echo},
  {"type", handle_type}
};

void adjust_out_stream(ParsedCommand& parsed_command) {
  std::vector<std::string> args = parsed_command.args;
  if(args.size() >= 3) {
    std::string second_last_args = args[args.size() - 2];
    if(second_last_args == ">" || second_last_args == "1>" ||
       second_last_args == "2>" ||
       second_last_args == ">>" || second_last_args == "1>>" ||
       second_last_args == "2>>") {

      std::string fname = args[args.size() - 1];

      int new_fd;
      if(second_last_args == ">>" || second_last_args == "1>>" || second_last_args == "2>>") {
      // set falgs to make it write only, create if non-existent and append 0644 for permissions
        new_fd = open(fname.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
      }else {
      //set flags to make it write only, create if non-existent and truncate(erase and start fresh) 0644 sets permissions
      new_fd = open(fname.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      }
      //pop the > arg and the filename arg
      parsed_command.args.pop_back();
      parsed_command.args.pop_back();
      if(second_last_args == "2>" || second_last_args == "2>>") {
        dup2(new_fd, 2);
      }else {
        dup2(new_fd, 1); //change output stream to point to new file descriptor
      }
    }
  }
}

bool repl(const std::string& user_input) {

  ParsedCommand parsed_command = parse_command(user_input);
  if(parsed_command.command == "exit") {
    return false;
  }

  int saved_fd_1 = dup(1);
  int saved_fd_2 = dup(2);
  adjust_out_stream(parsed_command);
  auto handler_it = built_in_handlers.find(parsed_command.command);
  if(handler_it != built_in_handlers.end()) {
    handler_it->second(parsed_command);
  }else {
    handle_external(parsed_command, user_input);
  }
  dup2(saved_fd_1, 1);
  dup2(saved_fd_2, 2);
  return true;
}


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(true) {
    std::cout << "$ ";
    std::string user_input;
    std::getline(std::cin, user_input);
    if(user_input.empty()) {
      continue;
    }

    if(!repl(user_input)) {
      break;
    }
  }

  return 0;
}
