#pragma once
#include <string>
#include "include/PacketHandler.h"

class PacketEngine {
 public:
  PacketEngine(std::string interface, unsigned int id, 
               PacketHandler *packetHandler);
  /*
   * Send Packet Function
   */
  void send(char *msg, unsigned int size);

  /*
   * Receive Packet Function
   */
  void receive(char *msg);

  /*
   * Forward Packet Function (Switch)
   */
  void forward(char *msg, unsigned int size);

  /*
   * Get Interface name
   */
  std::string getInterface() const;

  /*
   * Get my id
   */
  unsigned int getId() const;

 private:
  unsigned int socketFd_;
  void initializeEngine();
  std::string interface_;
  unsigned int myId_;
  unsigned int interfaceIdx_;
  PacketHandler *packetHandler_;
};
