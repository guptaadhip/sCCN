#include <iostream>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <thread>
#include <vector>
#include <cstring>

#include "include/net.h"
#include "include/Host.h"
#include "include/Logger.h"
#include "include/Util.h"

using namespace std;

/*
*	Host - Class Constructor
*/

Host::Host(int myId){
	Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
		"Entering Host.");
	std::vector<std::thread> packetEngineThreads;

	myId_ = myId;
	KeywordToUniqueIdMap publisherKeywordData_;
	KeywordToUIdsMap subscriberKeywordData_;
	sendHelloCounter_ = 0;

	for(auto &entry : myInterface_.getInterfaceList()) {
		PacketEngine packetEngine(entry, myId_, &packetHandler_);
		std::pair<std::string, PacketEngine> ifPePair (entry, packetEngine);
		ifToPacketEngine_.insert(ifPePair);
	}

	for(auto it = ifToPacketEngine_.begin(); it != ifToPacketEngine_.end(); it++){
		packetEngineThreads.push_back(std::thread(&Host::startSniffing, this,
			it->first, &it->second));
	}

	/* Create the queue pairs to handle the different queues */
	packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
		((unsigned short) PacketType::REGISTRATION_ACK, &RegRespQueue_));
	packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
		((unsigned short) PacketType::REGISTRATION_NACK, 	&RegRespQueue_));
	packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
		((unsigned short) PacketType::DEREGISTRATION_ACK, &UnRegRespQueue_));
	packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
		((unsigned short) PacketType::DEREGISTRATION_NACK, &UnRegRespQueue_));
	packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
		((unsigned short) PacketType::SUBSCRIPTION_ACK,	&SubRespQueue_));
	packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
		((unsigned short) PacketType::SUBSCRIPTION_NACK, &SubRespQueue_));
	packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
		((unsigned short) PacketType::DESUBSCRIPTION_ACK, &UnSubRespQueue_));
	packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
		((unsigned short) PacketType::DESUBSCRIPTION_NACK, &UnSubRespQueue_));
	packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
		((unsigned short) PacketType::DATA, &dataQueue_));

	/* Create Queue Handlers */
	auto handleDataThread = std::thread(&Host::handleDataPackets, this);
	auto handleRegistrationResponseThread = std::thread(&Host::
		handleRegistrationResponsePacket, this);
	auto handleUnRegistrationResponseThread = std::thread(&Host::
		handleUnRegistrationResponsePacket, this);
	auto handleSubscribeResponseThread = std::thread(&Host::
		handleSubscribeResponsePacket, this);
	auto handleDeSubscribeResponseThread = std::thread(&Host::
		handleDeSubscribeResponsePacket, this);

	/* Create Threads */
	auto handleSendHelloThread = std::thread(&Host::sendHello, this);

	/* Thread for invoking userInterface functionality */ 
	auto userInterfaceThread = std::thread(&Host::userInterface, this);

	packetHandler_.processQueue(&packetTypeToQueue_);

	/* waiting for all the packet engine threads */
	for (auto& joinThreads : packetEngineThreads) joinThreads.join();
}

/*
* Thread to Sniff for interface and packet engine 
*/
void Host::startSniffing(std::string, PacketEngine *packetEngine){
	char packet[BUFLEN];
	while(true) {
		packetEngine->receive(packet);
	}
}

/*
* regsiterKeyword - Request to Register Keyword
*/
bool Host::regsiterKeyword(std::string keyword, unsigned int sequenceNo){
	unsigned int keywordLength=keyword.length();

	/* Check if the keyword is already registered */
	if(publisherKeywordData_.keywordExists(keyword)){
		/* Print Success Message on STDOUT */
		userInterfaceLock_.lock();
		std::cout << "Registered Keyword:: " << keyword << std::endl;
		userInterfaceLock_.unlock();
		return true;
	}

	/* Send packet (with random number) to register the keyword */
	char packet[PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength];
	bzero(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength);

	struct PacketTypeHeader header;
	header.packetType = PacketType::REGISTRATION_REQ;

	struct RequestPacketHeader requestPacketHeader;
	requestPacketHeader.hostId = myId_;
	requestPacketHeader.sequenceNo = sequenceNo;
	requestPacketHeader.len = keywordLength;

	/* Store the Thread-SeqNo */
	pair<unsigned int, std::string> newPair(sequenceNo, keyword);
	keywordSeq_.insert(newPair);

	/* Copy data into the packet */
	memcpy(packet, &header, PACKET_HEADER_LEN);
	memcpy(packet + PACKET_HEADER_LEN, &requestPacketHeader, 
	REQUEST_HEADER_LEN);
	memcpy(packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN, keyword.c_str(), 
	keywordLength);

	/* Continue sending packet every 2secs until we receive either NACK or ACK */
	while(keywordSeq_.count(sequenceNo) > 0){
		for (auto &entry : ifToPacketEngine_) {
			entry.second.send(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN + 
			keywordLength);
		}
		std::this_thread::sleep_for (std::chrono::seconds(2));
	}
	return true;
}

/*
* unRegsiterKeyword - Request to Un-Register Keyword
*/
bool Host::unRegsiterKeyword(std::string keyword, unsigned int sequenceNo){
	unsigned int keywordLength=keyword.length();

	/* Check if the keyword is already registered */
	if(!publisherKeywordData_.keywordExists(keyword)){
		/* Print Success Message on STDOUT */
		userInterfaceLock_.lock();
		std::cout << "Un-Registered Keyword:: " << keyword << std::endl;
		userInterfaceLock_.unlock();
		return true;
	}

	/* Send packet (with random number) to register the keyword */
	char packet[PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength];
	bzero(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength);

	struct PacketTypeHeader header;
	header.packetType = PacketType::DEREGISTRATION_REQ;

	struct RequestPacketHeader requestPacketHeader;
	requestPacketHeader.hostId = myId_;
	requestPacketHeader.sequenceNo = sequenceNo;
	requestPacketHeader.len = keywordLength;

	/* Store the Thread-SeqNo */
	pair<unsigned int, std::string> newPair(sequenceNo, keyword);
	keywordSeq_.insert(newPair);

	/* Copy data into the packet */
	memcpy(packet, &header, PACKET_HEADER_LEN);
	memcpy(packet + PACKET_HEADER_LEN, &requestPacketHeader, 
	REQUEST_HEADER_LEN);
	memcpy(packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN, keyword.c_str(), 
	keywordLength);

	/* Continue sending packet every 2secs until we receive either NACK or ACK */
	while(keywordSeq_.count(sequenceNo) > 0){
		for (auto &entry : ifToPacketEngine_) {
			entry.second.send(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN + 
			 keywordLength);
		}
		std::this_thread::sleep_for (std::chrono::seconds(2));
	}
	return true;
}


/*
* publishData - Request to Publish Data
*/
void Host::publishData(std::string keyword, std::string payload){
	/* Check if the keyword is already registered */
	if(!publisherKeywordData_.keywordExists(keyword)){
		/* Print Failure Message on STDOUT <Keyword not registered> */
		userInterfaceLock_.lock();
		std::cout << "ERROR:: Keyword: " << keyword << " does not exists." 
				<< std::endl;
		userInterfaceLock_.unlock();
		return;
	}

	unsigned int uniqueId = publisherKeywordData_.fetchUniqueID(keyword);
	unsigned int payloadLength=payload.length();

	/* TBD:: If the payload length is greater than 1470 */

	/* Send packet (with random number) */
	char packet[PACKET_HEADER_LEN + DATA_HEADER_LEN + payloadLength];
	bzero(packet, PACKET_HEADER_LEN + DATA_HEADER_LEN + payloadLength);

	struct PacketTypeHeader header;
	header.packetType = PacketType::DATA;

	struct DataPacketHeader dataPacketHeader;
	dataPacketHeader.uniqueId = uniqueId;
	dataPacketHeader.sequenceNo = sequenceNumberGen();
	dataPacketHeader.len = payloadLength;

	/* Copy data into the packet */
	memcpy(packet, &header, PACKET_HEADER_LEN);
	memcpy(packet + PACKET_HEADER_LEN, &dataPacketHeader, 
	DATA_HEADER_LEN);
	memcpy(packet + PACKET_HEADER_LEN + DATA_HEADER_LEN, payload.c_str(), 
	payloadLength);

	for (auto &entry : ifToPacketEngine_) {
		entry.second.send(packet, PACKET_HEADER_LEN + DATA_HEADER_LEN + 
			payloadLength);
	}
	return;
}

/*
* subscribeKeyword - Subscribe the keyword
*/
bool Host::subscribeKeyword(std::string keyword, unsigned int sequenceNo){
	unsigned int keywordLength=keyword.length();

	/* Check if the keyword is already registered */
	if(subscriberKeywordData_.keywordExists(keyword)){
		/* Print subscribe Success Message on STDOUT */
		userInterfaceLock_.lock();
		std::cout << "Subscribed Keyword:: " << keyword << std::endl;
		userInterfaceLock_.unlock();
		return true;
	}

	/* Send packet (with random number) to register the keyword */
	char packet[PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength];
	bzero(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength);

	struct PacketTypeHeader header;
	header.packetType = PacketType::SUBSCRIPTION_REQ;

	struct RequestPacketHeader requestPacketHeader;
	requestPacketHeader.hostId = myId_;
	requestPacketHeader.sequenceNo = sequenceNo;
	requestPacketHeader.len = keywordLength;

	/* Store the Thread-SeqNo */
	pair<unsigned int, std::string> newPair(sequenceNo, keyword);
	keywordSeq_.insert(newPair);

	/* Copy data into the packet */
	memcpy(packet, &header, PACKET_HEADER_LEN);
	memcpy(packet + PACKET_HEADER_LEN, &requestPacketHeader, 
	REQUEST_HEADER_LEN);
	memcpy(packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN, keyword.c_str(), 
	keywordLength);

	/* Continue sending packet every 2secs until we receive either NACK or ACK */
	while(keywordSeq_.count(sequenceNo) > 0){
		for (auto &entry : ifToPacketEngine_) {
			entry.second.send(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN + 
				keywordLength);
		}
		std::this_thread::sleep_for (std::chrono::seconds(2));
	}
	return true;
}

/*
* unSubscribeKeyword - Un-Subscribe the keyword
*/
bool Host::unSubscribeKeyword(std::string keyword, unsigned int sequenceNo){
	unsigned int keywordLength=keyword.length();

	/* Check if the keyword is already registered */
	if(!subscriberKeywordData_.keywordExists(keyword)){
		/* Print Un-Subscribe Success Message on STDOUT */
		userInterfaceLock_.lock();
		std::cout << "Un-Subscribed Keyword:: " << keyword << std::endl;
		userInterfaceLock_.unlock();
		return true;
	}

	/* Send packet (with random number) to register the keyword */
	char packet[PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength];
	bzero(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength);

	struct PacketTypeHeader header;
	header.packetType = PacketType::DESUBSCRIPTION_REQ;

	struct RequestPacketHeader requestPacketHeader;
	requestPacketHeader.hostId = myId_;
	requestPacketHeader.sequenceNo = sequenceNo;
	requestPacketHeader.len = keywordLength;

	/* Store the Thread-SeqNo */
	pair<unsigned int, std::string> newPair(sequenceNo, keyword);
	keywordSeq_.insert(newPair);

	/* Copy data into the packet */
	memcpy(packet, &header, PACKET_HEADER_LEN);
	memcpy(packet + PACKET_HEADER_LEN, &requestPacketHeader, 
	REQUEST_HEADER_LEN);
	memcpy(packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN, keyword.c_str(), 
	keywordLength);

	/* Continue sending packet every 2secs until we receive either NACK or ACK */
	while(keywordSeq_.count(sequenceNo) > 0){
		for (auto &entry : ifToPacketEngine_) {
			entry.second.send(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN + 
				keywordLength);
		}
		std::this_thread::sleep_for (std::chrono::seconds(2));
	}
	return true;
}

/*
* sendHello - Send Hello packets to switch
*/
void Host::sendHello(){
	while(true){
		/* Send Hello packets if only the Host is a Subscriber */
		if(sendHelloCounter_ > 0){
			char packet[PACKET_HEADER_LEN + HELLO_HEADER_LEN];
			bzero(packet, PACKET_HEADER_LEN + HELLO_HEADER_LEN);

			struct PacketTypeHeader header;
			header.packetType = PacketType::HELLO;

			struct HelloPacketHeader helloPacketHeader;
			helloPacketHeader.nodeId = myId_;

			memcpy(packet, &header, PACKET_HEADER_LEN);
			memcpy(packet + PACKET_HEADER_LEN, &helloPacketHeader, HELLO_HEADER_LEN);

			for (auto &entry : ifToPacketEngine_) {
				entry.second.send(packet, PACKET_HEADER_LEN + HELLO_HEADER_LEN);
			}
		}
	}
	return;
}

/*
* Handle Incoming Data Packets
*/
void Host::handleDataPackets() {
	/* first one needs to be removed */
	unsigned int uniqueId;
	//unsigned int sequenceNo;

	(void) dataQueue_.packet_in_queue_.exchange(0,std::memory_order_consume);
	while(true) {
		auto pending = dataQueue_.packet_in_queue_.exchange(0, 
		std::memory_order_consume);
		if( !pending ) { 
			std::unique_lock<std::mutex> lock(dataQueue_.packet_ready_mutex_);    
			if( !dataQueue_.packet_in_queue_) {
				dataQueue_.packet_ready_.wait(lock);
			}
			continue;
		}
		uniqueId = 0;
		struct DataPacketHeader dataPacketHeader;
		bcopy(pending->packet + PACKET_HEADER_LEN, &dataPacketHeader,
				DATA_HEADER_LEN);
		uniqueId = dataPacketHeader.uniqueId;
		//sequenceNo = dataPacketHeader.sequenceNo;

		char data[dataPacketHeader.len];
		bcopy(pending->packet + PACKET_HEADER_LEN + DATA_HEADER_LEN, data,									
		dataPacketHeader.len);
		//std::string keyword = subscriberKeywordData_.fetchKeyword(uniqueId);
		if(subscriberKeywordData_.uniqueIDExists(uniqueId)){
			/* Print the Data on the STDOUT */
			userInterfaceLock_.lock();
			std::cout << "Received Data:: " << data << std::endl;
			userInterfaceLock_.unlock();
		}
	}
}

/*
* Handle Incoming Registration Response Packets
*/
void Host::handleRegistrationResponsePacket(){
	(void) RegRespQueue_.packet_in_queue_.exchange(0,std::memory_order_consume);
	while(true) {
		auto pending = RegRespQueue_.packet_in_queue_.exchange(0, 
			std::memory_order_consume);
		if( !pending ) { 
			std::unique_lock<std::mutex> lock(RegRespQueue_.packet_ready_mutex_);    
			if(!RegRespQueue_.packet_in_queue_) {
				RegRespQueue_.packet_ready_.wait(lock);
			}
			continue;
		}
		struct PacketTypeHeader packetTypeHeader;
		bcopy(pending->packet, &packetTypeHeader,PACKET_HEADER_LEN);

		struct ResponsePacketHeader responsePacketHeader;
		bcopy(pending->packet + PACKET_HEADER_LEN, &responsePacketHeader,
		RESPONSE_HEADER_LEN);

		/* Check if destined for this Host and seqNo exists */
		if(responsePacketHeader.hostId != myId_) continue;
		if(keywordSeq_.count(responsePacketHeader.sequenceNo) <= 0) continue;

		std::string keyword = keywordSeq_[responsePacketHeader.sequenceNo];
		/* Remove the entry from the keywordSeq_ , to stop the request Thread */
		keywordSeqLock_.lock();
		keywordSeq_.erase(responsePacketHeader.sequenceNo);
		keywordSeqLock_.unlock();

		if(packetTypeHeader.packetType == PacketType::REGISTRATION_ACK){
			/* Save the keyword and UniqueID */
			publisherKeywordData_.addKeywordIDPair(keyword, 
			responsePacketHeader.uniqueId);
			/* Print Registration Success Message on STDOUT */
			userInterfaceLock_.lock();
			std::cout << "Registered Keyword:: " << keyword << std::endl;
			userInterfaceLock_.unlock();
		}else{
			/* Print Registration failure Message on STDOUT */
			userInterfaceLock_.lock();
			std::cout << "ERROR:: Unable to registered Keyword: " << keyword << std::endl;
			userInterfaceLock_.unlock();
		}
	}
}

/*
* Handle Incoming UnRegistration Response Packets
*/
void Host::handleUnRegistrationResponsePacket(){
	(void) UnRegRespQueue_.packet_in_queue_.exchange(0,std::memory_order_consume);
	while(true) {
		auto pending = UnRegRespQueue_.packet_in_queue_.exchange(0, 
		std::memory_order_consume);
		if( !pending ) { 
			std::unique_lock<std::mutex> lock(UnRegRespQueue_.packet_ready_mutex_);    
			if(!UnRegRespQueue_.packet_in_queue_) {
				UnRegRespQueue_.packet_ready_.wait(lock);
			}
			continue;
		}

		struct PacketTypeHeader packetTypeHeader;
		bcopy(pending->packet, &packetTypeHeader,PACKET_HEADER_LEN);

		struct ResponsePacketHeader responsePacketHeader;
		bcopy(pending->packet + PACKET_HEADER_LEN, &responsePacketHeader,
		RESPONSE_HEADER_LEN);

		/* Check if destined for this Host and seqNo exists */
		if(responsePacketHeader.hostId != myId_) continue;
		if(keywordSeq_.count(responsePacketHeader.sequenceNo) <= 0) continue;

		std::string keyword = keywordSeq_[responsePacketHeader.sequenceNo];
		/* Remove the entry from the keywordSeq_ , to stop the request Thread */
		keywordSeqLock_.lock();
		keywordSeq_.erase(responsePacketHeader.sequenceNo);
		keywordSeqLock_.unlock();

		if(packetTypeHeader.packetType == PacketType::DEREGISTRATION_ACK){
			/* Remove the keyword from Publisher */
			publisherKeywordData_.removeKeywordIDPair(keyword);
			/* Print UnRegistration Success Message on STDOUT */
			userInterfaceLock_.lock();
			std::cout << "Un-Registered Keyword:: " << keyword << std::endl;
			userInterfaceLock_.unlock();
		}else{
			/* Print UnRegistration failure Message on STDOUT */
			userInterfaceLock_.lock();
			std::cout << "ERROR:: Unable to Un-Register Keyword:: " << keyword 
				<< std::endl;
			userInterfaceLock_.unlock();
		}
	}
}

/*
* Handle Incoming Subscribe Response Packets
*/
void Host::handleSubscribeResponsePacket(){
	(void) SubRespQueue_.packet_in_queue_.exchange(0,std::memory_order_consume);
	while(true) {
		auto pending = SubRespQueue_.packet_in_queue_.exchange(0, 
		std::memory_order_consume);
		if( !pending ) { 
			std::unique_lock<std::mutex> lock(SubRespQueue_.packet_ready_mutex_);    
			if(!SubRespQueue_.packet_in_queue_) {
				SubRespQueue_.packet_ready_.wait(lock);
			}
			continue;
		}

		struct PacketTypeHeader packetTypeHeader;
		bcopy(pending->packet, &packetTypeHeader,PACKET_HEADER_LEN);

		struct ResponsePacketHeader responsePacketHeader;
		bcopy(pending->packet + PACKET_HEADER_LEN, &responsePacketHeader,
		RESPONSE_HEADER_LEN);

		/* Check if destined for this Host and seqNo exists */
		if(responsePacketHeader.hostId != myId_) continue;
		if(keywordSeq_.count(responsePacketHeader.sequenceNo) <= 0) continue;

		std::string keyword = keywordSeq_[responsePacketHeader.sequenceNo];
		/* Remove the entry from the keywordSeq_ , to stop the request Thread */
		keywordSeqLock_.lock();
		keywordSeq_.erase(responsePacketHeader.sequenceNo);
		keywordSeqLock_.unlock();

		if(packetTypeHeader.packetType == PacketType::SUBSCRIPTION_ACK){
			/* Save the keyword and UniqueID */
			subscriberKeywordData_.addKeyword(keyword, 
			responsePacketHeader.uniqueId);
			sendHelloCounter_++;
			/* Print Subscribe Success Message on STDOUT */
			userInterfaceLock_.lock();
			std::cout << "Subscribed Keyword:: " << keyword << std::endl;
			userInterfaceLock_.unlock();
		}else{
			/* Print Subscribe failure Message on STDOUT */
			userInterfaceLock_.lock();
			std::cout << "ERROR:: Unable to subscribe keyword:: " << keyword 
					<< std::endl;
			userInterfaceLock_.unlock();
		}
	}
}

/*
* Handle Incoming UnSubscribe Response Packets
*/
void Host::handleDeSubscribeResponsePacket(){
	(void) UnSubRespQueue_.packet_in_queue_.exchange(0,std::memory_order_consume);
	while(true) {
		auto pending = UnSubRespQueue_.packet_in_queue_.exchange(0, 
		std::memory_order_consume);
		if( !pending ) { 
			std::unique_lock<std::mutex> lock(UnSubRespQueue_.packet_ready_mutex_);    
			if(!UnSubRespQueue_.packet_in_queue_) {
				UnSubRespQueue_.packet_ready_.wait(lock);
			}
			continue;
		}
		struct PacketTypeHeader packetTypeHeader;
		bcopy(pending->packet, &packetTypeHeader,PACKET_HEADER_LEN);

		struct ResponsePacketHeader responsePacketHeader;
		bcopy(pending->packet + PACKET_HEADER_LEN, &responsePacketHeader,
		RESPONSE_HEADER_LEN);

		/* Check if destined for this Host and seqNo exists */
		if(responsePacketHeader.hostId != myId_) continue;
		if(keywordSeq_.count(responsePacketHeader.sequenceNo) <= 0) continue;

		std::string keyword = keywordSeq_[responsePacketHeader.sequenceNo];
		/* Remove the entry from the keywordSeq_ , to stop the request Thread */
		keywordSeqLock_.lock();
		keywordSeq_.erase(responsePacketHeader.sequenceNo);
		keywordSeqLock_.unlock();

		if(packetTypeHeader.packetType == PacketType::DESUBSCRIPTION_ACK){
			/* Remove the keyword from Subscriber */
			subscriberKeywordData_.removeKeyword(keyword);
			sendHelloCounter_--;
			/* Print Un-Subscribe Success Message on STDOUT */
			userInterfaceLock_.lock();
			std::cout << "Un-Subscribed Keyword:: " << keyword << std::endl;
			userInterfaceLock_.unlock();
		}else{
			/* Print Un-Subscribe failure Message on STDOUT */
			userInterfaceLock_.lock();
			std::cout << "ERROR:: Unable to Un-Subscribe keyword:: " << keyword 
					<< std::endl;
			userInterfaceLock_.unlock();
		}
	}
}

/* 
* userInterface - Interact with the User 
*/
void Host::userInterface(){
	char userInput,publishTypeInput,subscriberTypeInput;
	string keywordInput,dataInput;
	unsigned int seqNo;

	while(true){
		userInterfaceLock_.lock();
		std::cout << "Please enter 'P' to Publish" << std::endl;
		std::cout << "Please enter 'S' to Subscribe" << std::endl;
		std::cin >> userInput;
		userInterfaceLock_.unlock();

		switch(userInput){
			case 'P':
			case 'p':
				userInterfaceLock_.lock();
				std::cout << "Please enter 'R' to Register Keyword" << std::endl;
				std::cout << "Please enter 'U' to Un-registered Keyword" << std::endl;
				std::cout << "Please enter 'P' to Send Data" << std::endl;
				std::cin >> publishTypeInput;
				userInterfaceLock_.unlock();

				switch(publishTypeInput){
				case 'R':
				case 'r':
					userInterfaceLock_.lock();
					std::cout << "Please enter the keyword to Register"<< std::endl;
					std::cin >> keywordInput;
					userInterfaceLock_.unlock();
					/* Create Thread to register keyword */
					seqNo = sequenceNumberGen();
					std::thread(&Host::regsiterKeyword, this, keywordInput, seqNo);
					break;

				case 'U':
				case 'u':
					userInterfaceLock_.lock();
					std::cout << "Please enter the keyword to Un-Register"<< std::endl;
					std::cin >> keywordInput;
					userInterfaceLock_.unlock();
					/* Create Thread to un-register keyword */
					seqNo = sequenceNumberGen();
					std::thread(&Host::unRegsiterKeyword, this, keywordInput, seqNo);
					break;

				case 'P':
				case 'p':
					userInterfaceLock_.lock();
					std::cout << "Please enter the keyword for which to Publish Data" 
						<< std::endl;
					std::cin >> keywordInput;
					std::cout << "Please enter the data to Publish Data"<< std::endl;
					std::cin >> dataInput;
					userInterfaceLock_.unlock();
					/* Create Thread to Publish Data */
					seqNo = sequenceNumberGen();
					std::thread(&Host::publishData, this, keywordInput, dataInput);
					break;

				default:
					std::cout << "Wrong Input entered. Skipping the request."
						 << std::endl;
				}
			break;

			case 'S':
			case 's':
				userInterfaceLock_.lock();
				std::cout << "Please enter 'S' to Subscribe Keyword" << std::endl;
				std::cout << "Please enter 'U' to Un-Subscribe Keyword" << std::endl;
				std::cin >> subscriberTypeInput;
				userInterfaceLock_.unlock();

				switch(subscriberTypeInput){
					case 'S':
					case 's':
						userInterfaceLock_.lock();
						std::cout << "Please enter the keyword to Subscribe." << 
						"Please separate keywords using colon (:)" << std::endl;
						std::cin >> keywordInput;
						userInterfaceLock_.unlock();
						/* Create Thread to Subscribe keyword */
						seqNo = sequenceNumberGen();
						std::thread(&Host::subscribeKeyword, this, keywordInput, seqNo);
						break;

					case 'U':
					case 'u':
						userInterfaceLock_.lock();
						std::cout << "Please enter the keyword to Un-Subscribe." << 
						"Please separate keywords using colon (:)" << std::endl;
						std::cin >> keywordInput;
						userInterfaceLock_.unlock();
						/* Create Thread to Un-Subscribe keyword */
						seqNo = sequenceNumberGen();
						std::thread(&Host::unSubscribeKeyword, this, keywordInput, seqNo);
						break;

					default:
						std::cout << "Wrong Input entered. Skipping the request." 
							<< std::endl;
				}
			break;

			default:
				std::cout << "Wrong Input entered. Skipping the request." 
					<< std::endl;
		}
	}
}
