#include <iostream>
#include <string>
#include <cstring>
#include "include/net.h"
#include "include/Logger.h"

extern bool DEBUG_MODE;

int main(int argc, char *argv[]) {
  int myId;
  std::string role;
  if (argc < 3) {
    Logger::log(Log::CRITICAL, __FUNCTION__, __LINE__, "too few arguments");
  } else if (argc == 4) {
    if (strncmp(argv[3], "dbg", strlen(argv[3])) == 0) {
      DEBUG_MODE = true;
    }
  } else if (argc > 4) {
    Logger::log(Log::CRITICAL, __FUNCTION__, __LINE__, "too many arguments");
  }
  myId = atoi(argv[1]);
  role = argv[2];
  Logger::log(Log::INFO, __FUNCTION__, __LINE__, "Hello World");
  Logger::log(Log::DEBUG, __FUNCTION__, __LINE__, "This is me");
  return 0;
}
