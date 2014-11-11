#pragma once
#include "boost/random.hpp"
#include "boost/generator_iterator.hpp"
#include <limits>
#include <ctime>

/*
 * Random sequence number generator based on the time
 */
unsigned int sequenceNumberGen() {
  typedef boost::mt19937 RNGType;
  RNGType rng;
  rng.seed(time(NULL));
  boost::uniform_int<unsigned int> seqNo( 1, 
                            std::numeric_limits<unsigned int>::max());
  boost::variate_generator< RNGType, boost::uniform_int<unsigned int> > 
                            sequenceNo(rng, seqNo);
  return (unsigned int) sequenceNo();
}

/*
 * Unique Id Generator (minimum unique ID is 100) 
 * for the first time we need to pass 0 as the last unique ID
 */
unsigned int uniqueIdGenerator(unsigned int lastUniqueId) {
  if (lastUniqueId == 0) {
    return 100;
  } else {
    return lastUniqueId++;
  }
}
