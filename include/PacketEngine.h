#pragma once
#include <string>

class PacketEngine {
 public:
  PacketEngine(std::string interface, unsigned int id);
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
  void initializeEngine();
  std::string interface_;
  unsigned int myId_;
  unsigned int interfaceIdx_;
};
