#pragma once
#include <string>
#include <set>
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
   * Send Hello Messages to Switch
   */
  void sendHello();
  /*
   * Thead to manage Switch State based on the hello message received
   */
  void switchStateHandler();

  /*
   * Handle Switch Registration
   */
  void handleSwitchRegistration();

  /*
   * TBD : Handle Keyword Registration
   */
  void handleKeywordRegistration();

  /*
   * TBD : Handle Keyword Subscription
   */
  void handleKeywordSubscription();

  /*
   * TBD : Handle Keyword Registration
   */
  void handleNetworkUpdate();

 private:
  unsigned int myId_;
  std::unordered_map<std::string, PacketEngine> ifToPacketEngine_;
  MyInterfaces myInterfaces_;
  PacketHandler packetHandler_;
  PacketTypeToQueue packetTypeToQueue_;
  unsigned int lastUniqueId_;

  /* all switch based data structures */
  std::vector<unsigned int> switchList_;
  std::unordered_map<unsigned int, std::string> switchToIf_;
  std::unordered_map<unsigned int, bool> switchToHello_;
  std::unordered_map <unsigned int, int> switchToHelloCount_;

  /* 
   * Data Structure to store the keywords to unique ID mapping
   */
  std::unordered_map<std::string, unsigned int> keywordsToUniqueId_;
  /* 
   * Data Structure to store the keyword to unique IDs mapping
   */
  std::unordered_map<std::string, std::set<unsigned int> > keywordToUniqueIds_;
  /* 
   * Data Structure to store the keywords to subscriber count
  */
  std::unordered_map<std::string, unsigned int> keywordToCount_;
  /* 
   * Data Structure to store the keyword to list of publishers
  */
  std::unordered_map<std::string, std::set<unsigned int> > keywordToPublishers_;
  /* 
   * Data Structure to store the keyword to list of subscribers
  */
  std::unordered_map<std::string, std::set<unsigned int> > keywordToSubscribers_;

  /* 
   * Queues for the handler threads
   */
  Queue switchRegQueue_;
  Queue helloQueue_;
  Queue registrationQueue_;
  Queue subscriptionQueue_;
  Queue networkUpdateQueue_;
};
