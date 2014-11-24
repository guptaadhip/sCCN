#include <iostream>

#include "include/KeywordToUniqueIdMap.h"
#include "include/Logger.h"

using namespace std;

/*
*	keywordExists - Check if that keyword exists in the map
*/
bool KeywordToUniqueIdMap::keywordExists(string keyword){
	/*check if the keyword is present in map*/
	if (keywordID_.count(keyword)>0)	return true;
	else	return false;
}

/*
* addKeywordIDPair - Adds a keyword- UniqueID pair to the map
*/
bool KeywordToUniqueIdMap::addKeywordIDPair(string keyword, 
		unsigned int uniqueID){
	/* check if the pair exists already. If yes, return false*/
	if(keywordExists(keyword))	return false;

	/* create a pair of the keyword and the unique ID*/
	pair<string,unsigned int> newPair(keyword,uniqueID);
	/* insert this into the map*/
	keywordID_.insert(newPair);
	return true;
}

/*
* removeKeywordIDPair - Removes a keyword- UniqueID pair on basis of keyword
*/
bool KeywordToUniqueIdMap::removeKeywordIDPair(string keyword){
	/*if keyword is not present in map, return false*/
	if(!keywordExists(keyword))	return false;

	/*remove the keyword uniqueID pair*/
	keywordID_.erase(keyword);
	return true;
}

/*
* fetchUniqueID - Fetches Unique ID on basis of keyword
*/
unsigned int KeywordToUniqueIdMap::fetchUniqueID(string keyword){
	/*if the required keyword does not exist, return 0*/
	if(!keywordExists(keyword)) return 0;
	return keywordID_[keyword];
}

/*
* fetchKeyword - Fetches Keyword on basis of UniqueID
*/
string KeywordToUniqueIdMap::fetchKeyword(unsigned int uniqueID){
	/*if the required keyword does not exist, return 0*/
	for ( auto it = keywordID_.begin(); it!= keywordID_.end(); ++it ){
		if(it->second == uniqueID){
			return it->first;
		}
	}
	return "";
}

std::unordered_map<std::string, unsigned int> KeywordToUniqueIdMap::getList() {
  return keywordID_;
}

/*
* displayMap - Print all the data from the map
*/
void KeywordToUniqueIdMap::displayMap(){
	/*display the entire map*/
	for ( auto it = keywordID_.begin(); it!= keywordID_.end(); ++it ){
		//Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__, it->first + ": "
		//	+ it->second);
	}
}
