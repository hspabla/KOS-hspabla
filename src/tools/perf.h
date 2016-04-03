#ifndef PERF_INIT_H
#define PERF_INIT_H

#include "ringbuf.h"
#include "hash.h"

#include "runtime/Thread.h"
#include "kernel/AddressSpace.h"
#include "kernel/Clock.h"
#include "kernel/Output.h"
#include "world/Access.h"
#include "machine/Machine.h"

#include <vector>

using namespace std;
static int QLIMIT = 100;
static int TOTAL_SAMPLES = 10000;

class Perf {
private:
	bool data_ready_flag;
	HashMap* hash_perf;
	vector<Queue> per_core_perf;
	void data_struct_init();
	void timer_init();
	void timer_clear();


public:
	Perf() { 
		data_ready_flag = false; 
		hash_perf = new HashMap();
		}
	~Perf() { delete hash_perf; }
	void put_sample(sampleD);
	void start();
	bool is_data_ready();
	void stop();
	void print();
	void print_core_buf();
};

#endif
