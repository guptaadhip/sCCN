#include <string>
#include <unordered_map>
#include "include/MyInterfaces.h"
#include "include/PacketEngine.h"
#include "include/PacketHandler.h"
#include "include/Queue.h"
#include "include/net.h"

class Switch{
 public:
  Switch(unsigned int myId);
  /*
   * Send hello
   */
  void sendHello();
  /*
   * Send Registration Packet on all interfaces till 
   * ack is not received. Every 2 seconds the packet will be sent
   */
  void sendRegistration();
  /*
   * Handle Hello Messages from Nodes [Switches/Controller]
   */
  void handleHello();

  /*
   * Thead to manage Switch State based on the hello message received
   */
  void nodeStateHandler();
  
  /*
   * Function to send Network Update to Controller
   */
  void sendNetworkUpdate(UpdateType, unsigned int, std::string);

  /*
   * Handle Registration Response from Controller
   */
  void handleRegistrationResp();

  /*
   * Handle Control Request Packets from host
   */
  void handleControlRequest();

  /*
   * Handle Data Packets from host
   */
  void handleData();

  /*
   * Handle Rule update packets from controller
   */
  void handleRuleUpdate();

  /*
   * Start receiving from interfaces
   */
  void startSniffing(std::string myInterface, PacketEngine *packetEngine);

  /*
   * Print forwarding table
   */
  void printForwardingTable();

 private:
  std::unordered_map<std::string, PacketEngine> ifToPacketEngine_;
  MyInterfaces myInterface_;
  unsigned int myId_;
  unsigned int myController_;
  std::string controllerIf_;
  PacketHandler packetHandler_;
  bool registered_;
  PacketTypeToQueue packetTypeToQueue_;

  std::vector<unsigned int> nodeList_;
  std::unordered_map<unsigned int, bool> nodeToHello_;
  std::unordered_map <unsigned int, int> nodeToHelloCount_;
  std::unordered_multimap<unsigned int, std::string> forwardingTable_;

  /* Anuj : data structures */
  std::vector<unsigned int> connectedSwitchList_;
  std::vector<unsigned int> connectedHostList_;
  /* a map from unique ID of node (may be host or switch) to interface */
  std::unordered_map<unsigned int, std::string> nodeUIdToIf_;
  
  /*
   * Queues for the handler threads
   */
  Queue switchRegRespQueue_;
  Queue helloQueue_;
  Queue controlRequestQueue_;
  Queue dataQueue_;
  Queue ruleQueue_;
};
