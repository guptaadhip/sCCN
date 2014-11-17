#pragma once
#include <unordered_map>
#include <string>
#include <vector>

	/* 
	* Used by the controller (and may be the host) ->
	* Keyword to Unique Ids Map Class
	* For example ->	{M} -> 91,92,93
	*/

class KeywordToUIdsMap {
	public:
	/* keywordExists - Check if that keyword exists in the map */
	bool keywordExists(std::string keyword);

	/*
	* If the keyword exists in the map, update the UID list of this keyword.
	* else add the keyword and the given UID (this would be the first pair).
	* addKeywordIDPair - Adds a keyword- UniqueID pair to the map
	*/
	void addKeyword(std::string keyword,unsigned int uniqueID);

	/* removeKeyword - Removes the whole entry for a particular keyword. */
	bool removeKeyword(std::string keyword);

	/* fetchUniqueID - Fetches Unique ID on basis of keyword */
	std::vector<unsigned int> fetchAllUIds(std::string);

	/* uniqueIDExists - Check if Unique ID Exists */
	bool uniqueIDExists(unsigned int uniqueID);

	/* displayMap - Print all the data from the map */
	void displayMap();

	private:
	std::unordered_map<std::string, std::vector<unsigned int>> keywordToUIds_;
	std::unordered_map<unsigned int,int> uniqueIDCounterMap_;
};
