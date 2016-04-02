#include <iostream>
#include <string>
#include "helper.h"

using namespace std;


class HashEntry {
private:
      uint64_t key;
      int counter;
public:
      HashEntry(uint64_t key, int counter) {
            this->key = key;
            this->counter = counter;
      }
 
      int getKey() {
            return key;
      }
 
      int getcounter() {
            return counter;
      }
};

const int TABLE_SIZE = 128;
 
class HashMap {
private:
      HashEntry **table;
public:
      HashMap() {
            table = new HashEntry*[TABLE_SIZE];
            for (int i = 0; i < TABLE_SIZE; i++)
                  table[i] = NULL;
      }
 
      int get(int key) {
            int hash = (key % TABLE_SIZE);
            while (table[hash] != NULL && table[hash]->getKey() != key)
                  hash = (hash + 1) % TABLE_SIZE;
            if (table[hash] == NULL)
                  return -1;
            else
                  return table[hash]->getcounter();
      }
 
      void put(int key, int counter) {
            int hash = (key % TABLE_SIZE);
            while (table[hash] != NULL && table[hash]->getKey() != key)
                  hash = (hash + 1) % TABLE_SIZE;
            if (table[hash] != NULL)
                  delete table[hash];
            table[hash] = new HashEntry(key, counter);
      }     
 
      ~HashMap() {
            for (int i = 0; i < TABLE_SIZE; i++)
                  if (table[i] != NULL)
                        delete table[i];
            delete[] table;
      }
};
