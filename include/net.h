#pragma once
#ifndef __NET_H_INCLUDED__
#define __NET_H_INCLUDED__
#include <string>

static const int BUFLEN = 1470;
/*
 * Task Structure
 */

struct packet {
  char packet[BUFLEN];
  std::string interface;
  struct packet *next;
};

/*
 * Packet Types
 */
enum class PacketType: unsigned short {
  REGISTRATION_REQ = 0xC0,
  REGISTRATION_ACK = 0xC1,
  REGISTRATION_NACK = 0xC2,
  SUBSCRIPTION_REQ = 0xC3,
  SUBSCRIPTION_ACK = 0xC4,
  SUBSCRIPTION_NACK = 0xC5,
  DEREGISTRATION_REQ = 0xC6,
  DEREGISTRATION_ACK = 0xC7,
  DEREGISTRATION_NACK = 0xC8,
  DESUBSCRIPTION_REQ = 0xC9,
  DESUBSCRIPTION_ACK = 0xCA,
  DESUBSCRIPTION_NACK = 0xCB,
  HELLO = 0xCC,
  DATA = 0xCD,
  NETWORK_UPDATE = 0xCE,
  SWITCH_REGISTRATION = 0xCF,
  SWITCH_REGISTRATION_ACK = 0xD1,
};
/*
 * Type of the update packet sent from Switch to the controller
 * and controller to switch (rule installation) 
 */
enum class UpdateType : unsigned char {
  ADD = 0xA,
  DELETE = 0xB,
};

/*
 * Common Packet header which tells the type of packet
 */
struct PacketTypeHeader {
  PacketType packetType;
};

/*
 * Structure of the Request packet from host to controller
 */
struct RequestPacketHeader {
  unsigned int sequenceNo;
  unsigned int hostId;
  unsigned int len;
};

/*
 * Structure to update the rules in the switch
 */
struct RuleUpdatePacketHeader {
  UpdateType type;
  unsigned int uniqueId;
  unsigned short port;
};

/*
 * Structure of the Hello Packet
 */
struct HelloPacketHeader {
  unsigned int nodeId;
};

/*
 * Structure of the Network Update packet from switch to Controller
 */
struct NetworkUpdatePacketHeader {
  UpdateType type;
  unsigned int nodeId;
  unsigned short port;
};

/*
 * Structure of the Response packet from controller to host
 */
struct ResponsePacketHeader {
  unsigned int sequenceNo;
  unsigned int uniqueId;
};

/* 
 * Structure of the Data Packet
 */
struct DataPacketHeader {
  unsigned int sequenceNo;
  unsigned int uniqueId;
  unsigned int len;
};

/* Length of the Packet Headers */
static const int PACKET_HEADER_LEN = sizeof(PacketTypeHeader);
static const int REQUEST_HEADER_LEN = sizeof(RequestPacketHeader);
static const int RESPONSE_HEADER_LEN = sizeof(ResponsePacketHeader);
static const int DATA_HEADER_LEN = sizeof(DataPacketHeader);
static const int RULE_UPDATE_HEADER_LEN = sizeof(RuleUpdatePacketHeader);
static const int HELLO_HEADER_LEN = sizeof(HelloPacketHeader);
static const int NETWORK_UPDATE_HEADER_LEN = sizeof(NetworkUpdatePacketHeader);

#endif 

