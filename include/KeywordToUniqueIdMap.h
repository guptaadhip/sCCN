#pragma once
#include <unordered_map>
#include <string>

/* Ip Table Entry Object */
class KeywordToUniqueIdMap {
	public:
	/* keywordExists - Check if that keyword exists in the map */
	bool keywordExists(std::string keyword);

	/* addKeywordIDPair - Adds a keyword- UniqueID pair to the map */
	bool addKeywordIDPair(std::string keyword,unsigned int uniqueID);

	/* removeKeywordIDPair - Removes a keyword- UniqueID pair on basis of keyword*/
	bool removeKeywordIDPair(std::string keyword);

	/* fetchUniqueID - Fetches Unique ID on basis of keyword 	*/
	unsigned int fetchUniqueID(std::string keyword);

	/* fetchKeyword - Fetches Keyword on basis of Unique ID */
	std::string fetchKeyword(unsigned int uniqueID);

	/* displayMap - Print all the data from the map */
	void displayMap();

	private:
	std::unordered_map<std::string, unsigned int> keywordID_;
};
