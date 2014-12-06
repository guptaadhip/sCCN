#pragma once
#include <mutex>
#include <string>
#include "include/MyInterfaces.h"
#include "include/PacketEngine.h"
#include "include/PacketHandler.h"
#include "include/KeywordToUniqueIdMap.h"
#include "include/KeywordToUIdsMap.h"
#include "include/Queue.h"

/*
* Class Host - Main Handler
*/
class Host {
  public:
  Host(int myId);

  /* 
   * Registration thread 
   */
  void keywordRegistrationHandler();

  /* 
   * Deregistration thread 
   */
  void keywordDeregistrationHandler();

  /* 
   * Subscription thread 
   */
  void keywordSubscriptionHandler();

  /*
  *  Unsubscription handler
  */
  void keywordUnsubscriptionHandler();

  /*
   * Queue Keyword Registration
   */
  void queueKeywordRegistration(PacketEntry *t);

  /*
   * Queue Keyword Deregistration
   */
  void queueKeywordDeregistration(PacketEntry *t);

  /*
   * Queue Keyword Subscription
   */
  void queueKeywordSubscription(PacketEntry *t);

  /*
   * Queue Keyword Unsubscription
  */
  void queueKeywordUnsubscription(PacketEntry *t);

  /*
   * Handle Control Packet Response
   */
  void handleControlResp();

  /*
   * Send the publishing map
   */
  std::unordered_map<std::string, unsigned int> getPublishingMap();
 
  private:
  unsigned int myId_;
  unsigned int sendHelloCounter_;
  unsigned int mySwitch_;
  std::string switchIf_;
  unsigned int switchDownCount_;
 
  MyInterfaces myInterface_;
  KeywordToUniqueIdMap publisherKeywordData_;
  KeywordToUIdsMap subscriberKeywordData_;
 
  PacketHandler packetHandler_;
  PacketTypeToQueue packetTypeToQueue_;
 
  std::unordered_map<std::string, PacketEngine> ifToPacketEngine_;

	std::mutex keywordSeqLock_;

  /* handle the host to switch registration response */
  void handleRegistrationResp();
  /* handle hello messages from switch */
  void handleHello();
  /* SendRegistration */
  void sendRegistration();
	/* Handle Hello Packets */
	void handleHelloPackets();
  /* Switch State Handler */
  void switchStateHandler();

  /* Map for registration ACK and NACK book-keeping */
  std::unordered_map<unsigned int, std::string> regAckNackBook_;
  /* Map for deregistration ACK and NACK book-keeping */
  std::unordered_map<unsigned int, unsigned int> deRegAckNackBook_;
  /* Map for subscription ACK and NACK book-keeping */
  std::unordered_map<unsigned int, std::string> subsAckNackBook_;
  /* Map for unsubscription ACK and NACK book-keeping */
  std::unordered_map<unsigned int, std::string> unsubsAckNackBook_;

  /* Handle Incoming Data Packets */
  void handleDataPackets();
  /* Handle Incoming Registration Response Packets */
  void handleRegistrationResponsePacket();
  /* Handle Incoming UnRegistration Response Packets */
  void handleUnRegistrationResponsePacket();
  /* Handle Incoming Subscribe Response Packets */
  void handleSubscribeResponsePacket();
  /* Handle Incoming UnSubscribe Response Packets */
  void handleDeSubscribeResponsePacket();
  /* sendHello - Send Hello packets to switch*/
  void sendHello();

  std::mutex userInterfaceLock_;
  /* userInterface - Interact with the User */
  void userInterface();

  /* regsiterKeyword - Request to Register Keyword */
  bool regsiterKeyword(std::string keyword, unsigned int sequenceNo);
  /* unRegsiterKeyword - Request to Un-Register Keyword */
  bool unRegsiterKeyword(std::string keyword, unsigned int sequenceNo);
  /* publishData - Request to Publish Data */
  void publishData(std::string keyword, std::string data);
  /* subscribeKeyword - Subscribe the keyword */
  bool subscribeKeyword(std::string keyword, unsigned int sequenceNo);
  /* unSubscribeKeyword - Un-Subscribe the keyword */
  bool unSubscribeKeyword(std::string keyword, unsigned int sequenceNo);

  /* Thread to Sniff for interface and packet engine */
  void startSniffing(std::string, PacketEngine *packetEngine);

  bool registered_;

  /*
   * Queues for the handler threads
  */
  Queue controlPacketQueue_;
  Queue dataQueue_;
  Queue hostRegRespQueue_;
  Queue helloQueue_;
  Queue registerQueue_;
  Queue deregisterQueue_;
  Queue subscriberQueue_;
  Queue unsubscriberQueue_;
};
