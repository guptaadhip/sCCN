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
   * Handle Hello Messages from Switch
   */
  void handleHello();

  /*
   * Thead to manage Switch State based on the hello message received
   */
  void switchStateHandler();

  /*
   * Handle Switch Registration
   */
  void handleSwitchRegistration();

 private:
  unsigned int myId_;
  std::unordered_map<std::string, PacketEngine> ifToPacketEngine;
  MyInterfaces myInterfaces_;
  PacketHandler packetHandler_;
  PacketTypeToQueue packetTypeToQueue_;

  /* all switch based data structures */
  std::vector<unsigned int> switchList_;
  std::unordered_map<unsigned int, std::string> switchToIf_;
  std::unordered_map<unsigned int, bool> switchToHello_;
  std::unordered_map <unsigned int, int> switchToHelloCount_;
  /* 
   * Queues for the handler threads
   */
  Queue regQueue_;
  Queue switchRegQueue_;
  Queue helloQueue_;
};
