#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <functional>
#include <unistd.h>
#include <vector>
#include<fcntl.h>
#include<termios.h>
#include<filesystem>

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif
const std::unordered_set<std::string> built_in_commands = {"echo", "type", "exit", "complete"};

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


// command -->exec script for command
std::unordered_map<std::string, std::string> completeMap;
void handle_complete(const ParsedCommand& parsed_command) {
  std::vector<std::string> args = parsed_command.args;
  if(args.size() == 0) {
    return;
  }

  //support for -p option of complete command
  auto it = std::find(args.begin(), args.end(), "-p");
  size_t idx = it - args.begin();
  if(idx < args.size() - 1) {
    auto it = completeMap.find(args[idx + 1]);
    if(it == completeMap.end()) {
      std::cout << "complete: " << args[idx + 1] << ": no completion specification" << std::endl;
    }else {
      std::cout << "complete -C '" << it->second << "' " << it->first << std::endl;
    }
    return;
  }

  // support for -C option of complete command
  it = std::find(args.begin(), args.end(), "-C");
  idx = it - args.begin();
  if(idx < args.size() - 2 /*&& access((*(it + 1)).c_str(), X_OK) == 0*/) {
    std::string keyCommand = *(it + 2);
    std::string valuePath = *(it + 1);
    completeMap[keyCommand] = valuePath;
    return;
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
  {"type", handle_type},
  {"complete", handle_complete}
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


std::set<std::string> matched_commands_set;
std::string longest_common_prefix_string() {
  if(matched_commands_set.size() == 0) {
    return "";
  }
  //for a sorted set of strings
  //the lcp string is the lcp of the first and last strings in the set
  std::string first = *(matched_commands_set.begin());
  std::string last = *(matched_commands_set.rbegin());

  size_t i = 0;
  while(true) {
    if(i == first.size() || i == last.size()) {
      break;
    }
    if(first[i] != last[i]) {
      break;
    }
    i++;
  }
  std::string lcp = first.substr(0, i);
  return lcp;
}
bool auto_completion_handler(std::string& user_input, bool second_consecutive_tab) {
  
  if(user_input.empty() || user_input.back() == ' ') {
    std::cout << "\a" << std::flush; // ring bell
    return false; // no need to wait for second tab. no results...so equivalent to result being flushed
  }

  if(second_consecutive_tab == true) {
    if(matched_commands_set.size() > 1) {
      std::cout << std::endl;
      for(const auto& match : matched_commands_set) {
        std::cout << match << "  " << std::flush;
      }
      std::cout << std::endl << "$ " << user_input << std::flush;
    }else {
      // 0 matches --> ring the bell
      std::cout << "\a" << std::flush;
    }
    matched_commands_set.clear();
    return false;
  }

  //find matches in built-in-commmands
  for(const std::string& built_in_command : built_in_commands) {
    if(built_in_command.starts_with(user_input)) {
      matched_commands_set.insert(built_in_command);
    }           
  }
  //find matches in exec(binaries)
  char* path_env = std::getenv("PATH");
  if(path_env) {
    std::stringstream path_ss(path_env);
    std::string dir;
    while(std::getline(path_ss, dir, PATH_LIST_SEPARATOR)) {
      std::error_code dir_ec;
      if(!std::filesystem::is_directory(dir, dir_ec) || dir_ec) {
        continue;
      }

      std::error_code it_ec;
      std::filesystem::directory_iterator it(dir, it_ec);
      if(it_ec) {
        // couldn't open this directory; skip it
        continue;
      }

      for(; it != std::filesystem::directory_iterator(); it.increment(it_ec)) {
        if(it_ec) {
          // skip this problematic entry and continue iterating
          it_ec.clear();
          continue;
        }

        std::error_code st_ec;
        const auto p = it->path();
        if(!std::filesystem::is_regular_file(p, st_ec) || st_ec) {
          continue;
        }
        if(access(p.string().c_str(), X_OK) != 0) {
          continue;
        }
        std::string local_fname = p.filename().string();
        if(local_fname.rfind(user_input, 0) == 0) {
          // a match
          matched_commands_set.insert(local_fname);
        }
      }
    }
  }

  //std::cout << "matchsize: " << matched_commands_set.size() << std::endl;
  if(matched_commands_set.size() == 1) {
    user_input = *(matched_commands_set.begin()) + " ";
    std::cout << "\r$ " << user_input << std::flush;
    matched_commands_set.clear();
    return false; //false means -->flushed.. so no scope for second tab
  }else if(matched_commands_set.size() > 1) {
   //extend suggestion to LCP. then on next tab display all the options

    //dont need to send matched_commands_list as parameter because it is a global variable
    std::string lcp = longest_common_prefix_string();
    if(lcp.size() > user_input.size()) {
      user_input = lcp;
      std::cout << "\r$ " << user_input << std::flush; //dont add space, because LCP string may not be a suggestion
      return false;
    }else {
      std::cout << "\a" << std::flush;
    }
    return true;
  }else {
    //ring the bell on the first tab if no matches
    std::cout << "\a" << std::flush;
    return true;
  }
}

std::set<std::string> run_completer_script(const ParsedCommand& parsed_command, const std::string& user_input) {
  std::set<std::string> completer_script_results;
  auto it = completeMap.find(parsed_command.command);
  if(it == completeMap.end()) {
    return completer_script_results;
  }
  const char* completer_script = (it->second).c_str();
  std::string exec = (it->second) + " " + parsed_command.command; //argv[0] --> command
  const std::vector<std::string>& args = parsed_command.args;
  if(args.size() > 0) {
    exec += " " + args.back() + " ";  //argv[1] --> word being completed
  }
  if(args.size() > 1) {
    exec += args[args.size() - 2]; //argv[2] --> preceeding word being completed
  }else if(args.size() == 1) {
    exec += parsed_command.command;
  }
  // "r" mode makes sure you can only read from the pipe
  // the child process triggered with completer_script, whatever it writes onto its std out stream
  // we can read from it only.. with "w" we could write to child's input stream and drive it through
  // a series of commands.


  std::string point_str = std::to_string(user_input.size());
  setenv("COMP_LINE", user_input.c_str(), 1);
  setenv("COMP_POINT", point_str.c_str(), 1);

  FILE* pipe = popen(exec.c_str(), "r");
  char buffer[256];
  while(fgets(buffer, sizeof(buffer), pipe)) {
    std::string line = buffer;
    if(line.empty()) continue;
    if(line.back() == '\n') {
      line.pop_back();
    }
    if(line.empty()) continue;
    completer_script_results.insert(line);
  }
  pclose(pipe);
  unsetenv("COMP_LINE");
  unsetenv("COMP_POINT");
  return completer_script_results;
}

void completer_auto_complete(std::set<std::string>& candidates, ParsedCommand& parsed_command, const std::string& user_input, bool& consecutive_completer_tab) {

  if(candidates.size() == 1) {
    //move cursor behind. So we can override the half completed argument
    //with completer script reply stored in candidates set
    if(parsed_command.args.size() > 0) {
      int delSize = parsed_command.args.back().size();
      for(int i = 0; i < delSize; i++) {
        std::cout << "\b";
      }
    }
    //write
    std::cout << *candidates.begin() << " " << std::flush;
  }else if(candidates.size() > 1) {
    if(!consecutive_completer_tab) {
      std::cout << "\a" << std::flush;
    }else {
      std::cout << std::endl;
      for(const std::string& candidate : candidates) {
        std::cout << candidate << "  ";
      }
      std::cout << std::endl;
      std::cout << "$ " << user_input << std::flush;
    }
    consecutive_completer_tab = !consecutive_completer_tab;
  }
  return;
}
std::string register_keystrokes_for_command() {
  std::string user_input;
  char ch;
  bool second_consecutive_tab = false;

  std::set<std::string> candidates;
  bool completer_consecutive_tab = false;
  while(true) {
    read(STDIN_FILENO, &ch, 1);
    if(ch != '\t') {
      matched_commands_set.clear();
      second_consecutive_tab = false;
      candidates.clear();
      completer_consecutive_tab = false;
    }

    if(ch == '\n') { //for newline
      std::cout << std::endl << std::flush;
      break;
    }else if (ch == '\t') { //for tab...auto completion
      ParsedCommand parsed_command = parse_command(user_input);
      if(!completer_consecutive_tab) {
        candidates = run_completer_script(parsed_command, user_input);
        //std::cout << std::endl << "CANDIDATES SIZE: " << candidates.size() << std::endl;
      }
      completer_auto_complete(candidates, parsed_command, user_input, completer_consecutive_tab);
      if(candidates.size() == 0) {
        bool second_consecutive_tab = auto_completion_handler(user_input, second_consecutive_tab);
      }
    }else if(ch == 8 || ch == 127) { //for backspace/delete
      if(user_input.size() > 0) {
        user_input.pop_back();
        std::cout << "\b \b" << std::flush; // hello^ -> hell^o -> hell ^ -> hell^
      }
    }else {
      std::cout << ch;
      user_input.push_back(ch);
    }
  }
  return user_input;
}

void set_raw_terminal_mode_for_keystrokes() {
  termios original_termios;
  tcgetattr(STDIN_FILENO, &original_termios);
  termios raw_termios = original_termios;
  raw_termios.c_lflag = raw_termios.c_lflag & ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios);
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  set_raw_terminal_mode_for_keystrokes();
  while(true) {
    std::cout << "$ ";
    std::string user_input = register_keystrokes_for_command();
    if(user_input.empty()) {
      continue;
    }

    //repl stand for read, evaluate,process, loop
    // the standard design of any shell process
    if(!repl(user_input)) {
      break;
    }
  }

  return 0;
}
