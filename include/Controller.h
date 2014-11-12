#pragma once
#include <string>
#include <unordered_map>

#include "PacketHandler.h"
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

  /*
   * Thread to Sniff for interface and packet engine 
   */
  void startSniffing(std::string, PacketEngine *packetEngine);

 private:
  unsigned int myId_;
  std::unordered_map<std::string, PacketEngine> ifToPacketEngine;
  MyInterfaces myInterfaces_;
  PacketHandler packetHandler_;
};
