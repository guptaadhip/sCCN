#pragma once
#ifndef __NET_H_INCLUDED__
#define __NET_H_INCLUDED__

/*
 * Packet Types
 */
enum class PacketType: int {
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
  DATA = 0xCC,
};

/*
 * Common Packet header which tells the type of packet
 */
struct PacketTypeHeader {
  unsigned short packetType;
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

#endif 

