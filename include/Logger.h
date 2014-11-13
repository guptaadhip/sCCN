#pragma once
#include <string>

extern bool DEBUG_MODE;

enum class Log {
  INFO, // For all kinds of prints
  DEBUG, // For all debug information DEBUG flag needs to be enabled in make file
  WARN, // For all warning but the program does not exit
  CRITICAL, // For all critical errors after which the program should exit
};

class Logger {
 public:
   static void log(Log logType, std::string fileName, 
                   std::string function, int line, std::string msg);
};
