#include <string>
#include <unordered_map>
#include "include/MyInterfaces.h"
#include "include/PacketEngine.h"
#include "include/PacketHandler.h"
#include "include/Queue.h"
#include "include/net.h"

/* Structure used as value in the forawarding table map */
struct HostIfCount {
  std::string interface;
  int count;
};

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
   * Send all the network updates to the controller.
   * Used when the controller goes down and comes back again
   */
  void sendAllNetworkUpdate();

  /*
   * Function to send Network Update to Controller
   */
  void sendNetworkUpdate(UpdateType, unsigned int);

  /*
   * Handle Registration Messages from the host
   */
  void handleHostRegistration();

  /*
   * Handle Registration Response from Controller
   */
  void handleRegistrationResp();

  /*
   * Handle Control Response Packets from Controller
   */
  void handleControlResponse();

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

  /*
   * Return the switch Id
   */
  unsigned int getSwitchId() const;

  /*
   * Return the controller Id
   */
  unsigned int getControllerId() const;
  
  /*
   * Return the controller Interface
   */
  std::string getControllerIf() const;

  /*
   * Return nodeId to interface
   */
  std::unordered_map<unsigned int, std::string> getNodeIdToIf() const;
  
  /*
   * Returns the switch forwarding table
   */
  std::unordered_multimap<unsigned int, struct HostIfCount> getForwardingTable() const;

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
  std::unordered_map<unsigned int, int> nodeToHelloCount_;
  std::unordered_multimap<unsigned int, struct HostIfCount> forwardingTable_;
  std::vector<unsigned int> connectedSwitchList_;
  std::vector<unsigned int> connectedHostList_;
  /* a map from unique ID of node (may be host or switch) to interface */
  std::unordered_map<unsigned int, std::string> nodeIdToIf_;
  
  /*
   * Queues for the handler threads
   */
  Queue switchRegRespQueue_;
  Queue hostRegistrationReqQueue_;
  Queue helloQueue_;
  Queue controlRequestQueue_;
  Queue controlResponseQueue_;
  Queue dataQueue_;
  Queue ruleQueue_;
};
