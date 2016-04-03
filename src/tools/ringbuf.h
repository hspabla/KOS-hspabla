#ifndef RINGBUF_H
#define RINGBUF_H

#include<iostream>
#include<cstdlib>

#include "helper.h"
 
using namespace std;
 
class Queue{
    private:
        int head, tail, size, count = 0;
				//sampleD sampleData;
        //int *ringArray;
				sampleD ringArray[QUEUE_SIZE];
	
    public:
        Queue();
				~Queue();
        void push(sampleD);
        void print();
				int qsize();
				uint64_t view(int);
				void data_read();
};


#endif
