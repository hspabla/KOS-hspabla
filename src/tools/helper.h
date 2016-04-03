#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>
#include "../include/kostypes.h"

#define CPU0 0
#define CPU1 1
#define CPU2 2
#define CPU3 3

#define QUEUE_SIZE 128

typedef struct SampleData{
	uint64_t address;
	uint64_t cpuID;
 	int access_type; 
}sampleD;


#endif
