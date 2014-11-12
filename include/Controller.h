#pragma once
#include <string>
#include <unordered_map>

#include "PacketEngine.h"
#include "MyInterfaces.h"

class Controller {
 public:
  Controller(unsigned int id);
  /*
   * Function to receive hello from the switches
   */
  void recvHello();
  /*
   * Get my id
   */
  unsigned int getId() const;

 private:
  unsigned int myId_;
  std::unordered_map<std::string, PacketEngine> ifToPacketEngine;
  MyInterfaces myInterfaces_;
};
