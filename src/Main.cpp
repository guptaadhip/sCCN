#include <iostream>
#include <string>
#include "boost/program_options.hpp"
#include "include/net.h"
#include "include/Logger.h"

extern bool DEBUG_MODE;

int main(int argc, char *argv[]) {
  int myId;
  std::string role;
  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("dbg", "Enable Debug Mode")
      ("id", po::value<int>(&myId)->required(), "My Id")
      ("role", po::value<std::string>()->required(), "Role[Controller/Switch/Host]");
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    role = vm["role"].as<std::string>();
    if (vm.count("dbg")) {
      DEBUG_MODE = true;
      Logger::log(Log::DEBUG, __FUNCTION__, __LINE__, "This is me");
    }
    Logger::log(Log::INFO, __FUNCTION__, __LINE__, "Hello World");
  } catch(std::exception& e) {
    std::cerr << "Unhandled Exception: " << e.what() << std::endl;
    return 2;
  }
  return 0;
}
