#include <string>
#include <unordered_map>
#include "include/MyInterfaces.h"
#include "include/PacketEngine.h"
#include "include/PacketHandler.h"

class Switch{
 public:
  Switch(unsigned int switchId);
  /*Send hello*/
  void sendHello();
  /*Send "I am up" message*/
  void sendImUp();
  /*
   * Send Registration Packet on all interfaces till 
   * ack is not received. Every 2 seconds the packet will be sent
   */
  void sendRegistration();
  /*
   * Handle Registration Response from Controller
   */
  void handleRegistration();

 private:
  std::unordered_map<std::string, PacketEngine> ifToPacketEngine;
  MyInterfaces myInterface_;
  unsigned int switchId_;
  PacketHandler packetHandler_;
  bool registered_;
};
   
    


  
