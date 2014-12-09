#pragma once
#include <string>
#include <set>
#include <vector>
#include <unordered_map>
#include "net.h"
#include "Queue.h"
#include "PacketHandler.h"
#include "PacketEngine.h"
#include "MyInterfaces.h"
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/undirected_graph.hpp>

class Controller {
 public:
  Controller(unsigned int id);
  /* Get my id */
  unsigned int getId() const;

  /* Thread to Sniff for interface and packet engine */
  void startSniffing(std::string, PacketEngine *packetEngine);

  /* Handle Hello Messages from Switch */
  void handleHello();

  /* Send Hello Messages to Switch */
  void sendHello();
  
  /* Thead to manage Switch State based on the hello message received */
  void switchStateHandler();

  /* Handle Switch Registration */
  void handleSwitchRegistration();

  /* TBD : Handle Keyword Registration */
  void handleKeywordRegistration();

  /* TBD : Handle Keyword Subscription */
  void handleKeywordSubscription();

  /* TBD : Handle Keyword Registration */
  void handleNetworkUpdate();

  /*Install/Uninstall Rules on the Switches during subscription/unsub'tion*/
  void installRules(unsigned int, unsigned int, UpdateType);
	
	/* Install/Uninstall Rules on the Switches during registration/dereg */
  void installRulesRegistration(unsigned int, unsigned int, UpdateType);
  
  /*Print map(keywordToUniqueIds_)*/
  void printMap(std::unordered_map<std::string, std::set<unsigned int>> input);

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
  std::unordered_map<std::string, unsigned int> ifToSwitch_;
  std::unordered_map<unsigned int, bool> switchToHello_;
  std::unordered_map <unsigned int, int> switchToHelloCount_;

  /* Data Structure to store the keywords to unique ID mapping */
  std::unordered_map<std::string, unsigned int> keywordsToUniqueId_;
  
  /* Data Structure to store the keyword to unique IDs mapping */
  std::unordered_map<std::string, std::set<unsigned int> > keywordToUniqueIds_;

  /* Data Structure to store the uniqueId to list of publishers */
  std::unordered_map<unsigned int, std::set<unsigned int> > uniqueIdToPublishers_;
  /* Data Structure to store the uniqueId to list of subscribers */
  std::unordered_map<unsigned int, std::vector<unsigned int> > uniqueIdToSubscribers_;

  /* 
   * Queues for the handler threads
   */
  Queue switchRegQueue_;
  Queue helloQueue_;
  Queue registrationQueue_;
  Queue subscriptionQueue_;
  Queue networkUpdateQueue_;
  
  /*
   * Network Map
   */
  typedef boost::property<boost::edge_weight_t, float> weightProperty_;
  typedef boost::adjacency_list<
            boost::setS,      // out-edges stored in a std::list
            boost::vecS,      // vertex set stored here
            boost::undirectedS,      // undirected graph.
            boost::no_property,      // vertex properties
            weightProperty_,      // edge properties
            boost::no_property,      // graph properties
            boost::listS      // edge storage
      > graphStructure_;
      
  weightProperty_ weight_;
  typedef boost::graph_traits <graphStructure_>::vertex_descriptor 
   vertexDescriptor_;
  typedef boost::graph_traits <graphStructure_>::edge_descriptor edgeDescriptor_;
  typedef std::pair<int, int> edge_;
  
  graphStructure_ graph_;
 
};
