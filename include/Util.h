#pragma once
#include "boost/random.hpp"
#include "boost/generator_iterator.hpp"
#include <limits>
#include <ctime>

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
