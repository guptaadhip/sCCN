#pragma once
#ifndef __UTIL_H_INCLUDED__
#define __UTIL_H_INCLUDED__

/*
 * Random sequence number generator based on the time
 */
unsigned int sequenceNumberGen();

/*
 * Unique Id Generator (minimum unique ID is 100) 
 * for the first time we need to pass 0 as the last unique ID
 */
unsigned int uniqueIdGenerator(unsigned int lastUniqueId);
#endif 
