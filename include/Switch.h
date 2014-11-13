#include <string>
#include <unordered_map>
#include "include/MyInterfaces.h"
#include "include/PacketEngine.h"
#include "include/PacketHandler.h"
#include "include/Queue.h"

class Switch{
 public:
  Switch(unsigned int myId);
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
  void handleRegistrationResp();
  /*
   * Start receiving from interfaces
   */
  void startSniffing(std::string myInterface, PacketEngine *packetEngine);

 private:
  std::unordered_map<std::string, PacketEngine> ifToPacketEngine;
  MyInterfaces myInterface_;
  unsigned int myId_;
  unsigned int myController_;
  PacketHandler packetHandler_;
  bool registered_;

  PacketTypeToQueue packetTypeToQueue;
  /*
   * Queues for the handler threads
   */
  Queue switchRegRespQueue_;
};
   
    


  
