#pragma once
#ifndef __NET_H_INCLUDED__
#define __NET_H_INCLUDED__
#include <string>
#include <unordered_map>

static const int BUFLEN = 1470;

static const uint32_t CONTROL_NW_IP = 0x0000A8C0; /* Deter control Nw IP */

/*
 * Task Structure
 */
struct PacketEntry {
  char packet[BUFLEN];
  std::string interface;
  struct PacketEntry *next;
  unsigned int len;
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
  RULE = 0xD2,
  RULE_ACK = 0xD3,
  RULE_NACK = 0xD4,
  HOST_REGISTRATION = 0xD5,
  HOST_REGISTRATION_ACK = 0xD6,
  NETWORK_UPDATE_ACK = 0xD7,
  NETWORK_UPDATE_NACK = 0xD8
};
/*
 * Type of the update packet sent from Switch to the controller
 * and controller to switch (rule installation) 
 */
enum class UpdateType : unsigned char {
  ADD_SWITCH = 0xA0,
  DELETE_SWITCH = 0xB0,
  ADD_HOST = 0xA1,
  DELETE_HOST = 0xB1,
  ADD_RULE = 0xA2,
  DELETE_RULE = 0xB2,
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
 * Structure of the Response packet from controller to host
 */
struct ResponsePacketHeader {
  unsigned int sequenceNo;
  unsigned int hostId;
  unsigned int len;
};

/*
 * Packet Structure of the Switch Registration Packet to Controller
 */
struct RegistrationPacketHeader {
  unsigned int nodeId;
};

/*
 * Switch Registration Response Packet Structure from Controller
 */
struct RegistrationResponsePacketHeader {
  unsigned int nodeId;
};

/*
 * Structure to update the rules in the switch
 */
struct RuleUpdatePacketHeader {
  UpdateType type;
  unsigned int uniqueId;
  unsigned int nodeId;
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
  unsigned int seqNo;
  UpdateType type;
  unsigned int nodeId;
  char interface[10];
};

/*
 * Structure of the Network Update Response packet from Controller to Switch
 */
struct NetworkUpdateResponsePacketHeader {
  unsigned int seqNo;
  unsigned int nodeId;
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
static const int REGISTRATION_HEADER_LEN = sizeof(RegistrationPacketHeader);
static const int REGISTRATION_RESPONSE_HEADER_LEN = 
                                      sizeof(RegistrationResponsePacketHeader);
static const int NETWORK_UPDATE_RESPONSE_HEADER_LEN = 
                        sizeof(NetworkUpdateResponsePacketHeader);

#endif 

