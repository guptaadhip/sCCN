#pragma once
#include <vector>
#include <string>
#include <ifaddrs.h>

/*
 * Gets the list of all the local interfaces except loopback
 */
class MyInterfaces {
 public:
  MyInterfaces();
  /*
   * Get the interface list
   */
  std::vector<std::string> getInterfaceList() const;

  /*
   * Print the list of interfaces
   */
  void printInterfaceList();

 private:
  std::vector<std::string> interfaces_;
  struct ifaddrs *ifAddrStruct_= NULL;
};
