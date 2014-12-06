#include <iostream>
#include <algorithm>

#include "include/KeywordToUIdsMap.h"
#include "include/Logger.h"

using namespace std;

/*
* keywordExists - Check if that keyword exists in the map
* Used by the controller (and may be the host) ->
* Keyword to Unique Ids Map Class
* For example ->	{M} -> 91,92,93
*/

bool KeywordToUIdsMap::keywordExists(string keyword){
	/* check if the keyword is present in map */
	if (keywordToUIds_.count(keyword) > 0){
		return true;
	}else{
		return false;
	}
}

/*
* addKeywordIDPair - Adds a keyword- UniqueID pair to the map
*/
void KeywordToUIdsMap::addKeyword(string keyword, unsigned int uniqueID) {
	if(!keywordExists(keyword)) {
		/* Create uID Vector */
		vector<unsigned int> uidVector;
		uidVector.push_back(uniqueID);

		/*If uniqueID existed with another keyword*/
		if (uniqueIDCounterMap_.count(uniqueID) > 0){
			uniqueIDCounterMap_[uniqueID]+=1;
		}
		
		/* Create New Pair Keyword-Vector<UID> */   
		pair<string,vector<unsigned int>> newPair(keyword,uidVector);
		keywordToUIds_.insert(newPair);

		/* Insert UID into UIDCounter Map */
		pair<unsigned int,int> newpair(uniqueID,1);
		uniqueIDCounterMap_.insert(newpair);
	}else{	/*If keyword exists*/
		if(find(keywordToUIds_[keyword].begin(), 
			keywordToUIds_[keyword].end(),uniqueID) != keywordToUIds_[keyword].end()){
			return;
		}

		/* Insert New UID into existing Keyword-Vector<UID> */
		keywordToUIds_[keyword].push_back(uniqueID);

		/* Insert UID into Vector */
		if (uniqueIDCounterMap_.count(uniqueID)>0){
			uniqueIDCounterMap_[uniqueID]+=1;
		}else{
			pair<unsigned int,int> newpair(uniqueID,1);
			uniqueIDCounterMap_.insert(newpair);
		}
	}
}

/*
* removeKeyword - Removes the whole entry for a particular keyword.
*/
bool KeywordToUIdsMap::removeKeyword(string keyword) {
	/* if keyword is present in map, remove the entry. */
	if(keywordExists(keyword)) {
		for ( auto it : keywordToUIds_[keyword]){
			/*Decrement the counter for the uniqueID that exists*/
			uniqueIDCounterMap_[it]-=1;
			if(uniqueIDCounterMap_[it] == 0){
				uniqueIDCounterMap_.erase(it);
			}
		}
		
		keywordToUIds_.erase(keyword);
		return true;
	}
	return false;
}

/*
* fetchUniqueID - Fetches Unique ID on basis of keyword
*/
vector<unsigned int> KeywordToUIdsMap::fetchAllUIds(string keyword) {
	return keywordToUIds_[keyword];
}

/*
* uniqueIDExists - Check if UID exists
*/
bool KeywordToUIdsMap::uniqueIDExists(unsigned int uniqueID){
	if (uniqueIDCounterMap_.count(uniqueID) > 0){
		return true;
	}else{
		return false;
	}
}

/*
* displayMap - Print all the data from the map
*/
void KeywordToUIdsMap::displayMap() {
	/* display the entire map */
	for ( auto it = keywordToUIds_.begin(); it!= keywordToUIds_.end(); ++it ){
		//Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__, it->first + ": ");

		for (auto it1 : it->second){
			//Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__, it1 + ": " );
		}
	}

	/*Display the uniqueID map*/
	for (auto it2 = uniqueIDCounterMap_.begin(); it2!= uniqueIDCounterMap_.end(); 
	++it2 ){
		//Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__, it2->first + ": ");
	}
}

std::unordered_map<std::string, std::vector<unsigned int>> KeywordToUIdsMap::getList() {
	return keywordToUIds_;
}
