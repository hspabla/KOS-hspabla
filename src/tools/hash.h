#ifndef HASH_H
#define HASH_H

#include <iostream>
#include <string>
#include "kernel/Output.h"
#include "helper.h"
#include <vector>

using namespace std;


class HashEntry {
private:
      uint64_t key;
      int counter;
public:
      HashEntry(uint64_t key) {
            this->key = key;
            this->counter = 0;
      }
      uint64_t getKey() {
            return key;
      }
      int getcounter() {
            return counter;
      }
      void putcounter(int counter) {
            this->counter = counter;
      }
      void putkey(uint64_t key) {
            this->key = key;
      }
};
const int TOP_SAMPLE = 10;
const int TABLE_SIZE = 512;
 
class HashMap {
private:
      vector<HashEntry> table;
public:
      HashMap() {
        for (int i = 0; i < TABLE_SIZE; i++)
           table.push_back(HashEntry(0));
      }
      
      int numberOfEntry() {
        int co=0;
        for (int i =0; i < TABLE_SIZE ; i++) {
            if (table[i].getKey() != 0) {
                co = co + table[i].getcounter();
       			}
				}
				return co;
     }

      void put(uint64_t key) {
        //   	KOUT::outl("Adding key in hash table: ", key);
						int hash = (key % TABLE_SIZE);
            while (table[hash].getKey() != 0 && table[hash].getKey() != key)
                  hash = (hash + 1) % TABLE_SIZE;
            if (table[hash].getKey() == 0) 
                  table[hash].putkey(key);
            table[hash].putcounter(table[hash].getcounter() + 1);
      }     
 
      void topTen() {
      	vector<HashEntry> top_samples;
				for (int i=0; i<TABLE_SIZE; i++) {
					top_samples.push_back(table[i]);
				}
				
				for (int i=0; i<TABLE_SIZE; i++) {
					for (int j=i+1; j<TABLE_SIZE; j++) {
						if (top_samples[i].getcounter() < top_samples[j].getcounter() ) {
							HashEntry temp(0);
							temp = top_samples[j];
							top_samples[j] = top_samples[i];
							top_samples[i] = temp; 
						}
					}
				}
				KOUT::outl("TOP ", TOP_SAMPLE , " SAMPLES");
				for (int i=0; i < TOP_SAMPLE ; i++) {
					KOUT::outl(top_samples[i].getcounter()," # of times " ,FmtHex(top_samples[i].getKey()));
				}
      }

			~HashMap() {
      }
};

#endif


