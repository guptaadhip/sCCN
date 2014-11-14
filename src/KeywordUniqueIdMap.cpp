#include "include/KeywordUniqueIdMap.h"
#include <cstring>
#include "include/Logger.h"

using namespace std;

/*
 * keywordExists - Check if that keyword exists in the map
 */
bool KeywordUniqueIdMap::keywordExists(string keyword) {
  /*
   * check if the keyword is present in map
   */
  if (keywordID_.count(keyword) > 0) { 
    return true;
  } else {
    return false;
  }
}

/*
 * addKeywordIDPair - Adds a keyword- UniqueID pair to the map
 */
bool KeywordUniqueIdMap::addKeywordIDPair(string keyword, unsigned int uniqueID) {
	/*
   * check if the pair exists already. If yes, return false
   */
  if(keywordExists(keyword)) {
    return false;
  }
	/*
   * create a pair of the keyword and the unique ID
   */
  pair<string,unsigned int> newPair(keyword,uniqueID);
	/*
   * insert this into the map
   */
  keywordID_.insert(newPair);
  return true;
}

/*
 * removeKeywordIDPair - Removes a keyword- UniqueID pair on basis of keyword
 */
bool KeywordUniqueIdMap::removeKeywordIDPair(string keyword) {
  /*
   * if keyword is not present in map, return false
   */
  if(!keywordExists(keyword)) {
    return false;
  }
  /*
   * remove the keyword uniqueID pair
   */
  keywordID_.erase(keyword);
  return true;
}

/*
 * fetchUniqueID - Fetches Unique ID on basis of keyword
 */
unsigned int KeywordUniqueIdMap::fetchUniqueID(string keyword) {
  /*
   * if the required keyword does not exist, return 0
   */
  if(!keywordExists(keyword)) {
    return 0;
  }
  return keywordID_[keyword];
}

/*
 * fetchUniqueID - Fetches Unique ID on basis of keyword
 */
string KeywordUniqueIdMap::fetchKeyword(unsigned int uniqueID) {
  /*
   * if the required keyword does not exist, return 0
   */
  for (auto &entry : keywordID_) {
    if(entry.second == uniqueID) {
      return entry.first;
    }
  }
  return "";
}

/*
 * displayMap - Print all the data from the map
 */
void KeywordUniqueIdMap::displayMap() {
  /*
   * display the entire map
   */
  for (auto &entry : keywordID_) {
    Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__,
                entry.first + ": " + std::to_string(entry.second));
  }
}
