#pragma once
#include <mutex>
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
 
 
 private:
 unsigned int myId_;
 unsigned int sendHelloCounter_;
 
 MyInterfaces myInterface_;
 KeywordToUniqueIdMap publisherKeywordData_;
 KeywordToUIdsMap subscriberKeywordData_;
 
 PacketHandler packetHandler_;
 PacketTypeToQueue packetTypeToQueue_;
 
 std::unordered_map<std::string, PacketEngine> ifToPacketEngine_;
 
 std::mutex keywordSeqLock_;
 /* Thread to sequence Number Map */
 std::unordered_map<unsigned int, std::string> keywordSeq_;
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
 
 /* 
 * Queues for the handler threads
 */
 Queue RegRespQueue_;
 Queue UnRegRespQueue_;
 Queue SubRespQueue_;
 Queue UnSubRespQueue_;
 Queue dataQueue_;
};
