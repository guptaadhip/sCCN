#include "include/Util.h"
#include <random>
#include <limits>
#include <ctime>

/*
 * Random sequence number generator based on the time
 */
unsigned int sequenceNumberGen() {
  typedef std::mt19937 RNGType;
  RNGType rng;
  rng.seed(time(NULL));
  std::uniform_int_distribution<unsigned int> seqNo(1, 
                            std::numeric_limits<unsigned int>::max());
  return (unsigned int) seqNo(rng);
}

/*
 * Unique Id Generator (minimum unique ID is 100) 
 * for the first time we need to pass 0 as the last unique ID
 */
unsigned int uniqueIdGenerator(unsigned int lastUniqueId) {
  if (lastUniqueId == 0) {
    return 100;
  } else {
    unsigned int uniqueId = lastUniqueId;
    uniqueId = uniqueId + 1;
    return uniqueId;
  }
}
