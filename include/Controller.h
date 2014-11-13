#pragma once
#include <string>
#include <unordered_map>
#include "net.h"
#include "Queue.h"
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


  /*
   * Handle Switch Registration
   */
  void handleSwitchRegistration();

 private:
  unsigned int myId_;
  std::unordered_map<std::string, PacketEngine> ifToPacketEngine;
  MyInterfaces myInterfaces_;
  PacketHandler packetHandler_;
  std::vector<unsigned int> switchList;
  std::unordered_map<unsigned int, std::string> switchToIf;
  PacketTypeToQueue packetTypeToQueue;

  /* 
   * Queues for the handler threads
   */
  Queue regQueue_;
  Queue switchRegQueue_;
};
