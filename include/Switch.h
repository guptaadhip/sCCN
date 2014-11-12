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

 private:
  std::unordered_map<std::string, PacketEngine> ifToPacketEngine;
  MyInterfaces myInterface_;
  unsigned int switchId_;
  PacketHandler packetHandler_;
};
   
    


  
