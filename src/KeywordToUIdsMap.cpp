#include "include/KeywordToUIdsMap.h"
#include <cstring>
#include <iostream>
#include "include/Logger.h"

using namespace std;

/*
 * keywordExists - Check if that keyword exists in the map
 * Used by the controller (and may be the host) ->
 * Keyword to Unique Ids Map Class
 * For example ->     {M} -> 91,92,93
*/

bool KeywordToUIdsMap::isKeywordPresent(string keyword) {
  /*
   * check if the keyword is present in map
   */
  if (keywordToUIds_.count(keyword) > 0) {
    return true;
  } else {
    return false;
  }
}

/*
 * If the keyword exists in the map, update the UID list of this keyword.
 * else add the keyword and the given UID (this would be the first pair).
 * addKeywordIDPair - Adds a keyword- UniqueID pair to the map
*/
void KeywordToUIdsMap::addKeywordToIdPair(string keyword, unsigned int UId) {
  if(!isKeywordPresent(keyword)) {
    keywordToUIds_[keyword];
  }
  keywordToUIds_[keyword].push_back(UId);
}

/*
 * removeKeyword - Removes the whole entry for a particular keyword.
*/
void KeywordToUIdsMap::removeKeyword(string keyword) {
  /*
   * if keyword is present in map, remove the entry.
   */
  if(isKeywordPresent(keyword)) {
    keywordToUIds_.erase(keyword);
  }
}

/*
 * fetchUniqueID - Fetches Unique ID on basis of keyword
 */
vector<unsigned int> KeywordToUIdsMap::fetchAllUIds(string keyword) {
  return keywordToUIds_[keyword];
}

/*
 * displayMap - Print all the data from the map
 */
void KeywordToUIdsMap::displayMap() {
  /*
   * display the entire map
   */
  for (auto &entry : keywordToUIds_) {
    Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__,
                entry.first + ": ");
    for (auto c : entry.second)
      cout << c << ' ';
    cout << std::endl;
  }
}
