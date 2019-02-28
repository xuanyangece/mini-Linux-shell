#include <stdio.h>

#include "xyproject.h"

extern char ** environ;

int main() {
  // input - stores input command every time user types
  // vars - stores all set variables
  std::string input;
  std::unordered_map<std::string, std::string> vars;

  // print shell information with current directory
  printShell();

  // define two variables to store environment variables
  std::vector<char *> envs;
  char * env_path = nullptr;

  // read from stdin
  while (std::getline(std::cin, input)) {
    // preset environ vars
    envs = setEnv(environ);
    env_path = getenv("PATH");

    // if input is only white space, then continue without and fork()
    if (isSpace(input)) {
      printShell();
      continue;
    }

    // if input is exit, then exit
    if (isExit(input))
      break;

    // prune input for potential variable and '\'
    input = pruneInput(input, vars);

    if (isBuiltIn(input)) { /* for build in instructions like cd */
      handleBuiltIn(envs, env_path, input, vars);
    }
    else { /* for real command, create process */
      handleProcess(fork(), envs, env_path, input);
    }
  }

  // print program information before exit
  std::cout << "Program exited with status " << EXIT_SUCCESS << std::endl;

  return EXIT_SUCCESS;
}
