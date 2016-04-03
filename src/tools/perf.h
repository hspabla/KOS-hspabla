#ifndef PERF_INIT_H
#define PERF_INIT_H
#include "ringbuf.h"
#include "hash.h"
#include "runtime/Scheduler.h"
#include "runtime/Thread.h"
#include "kernel/Output.h"
#include "machine/Machine.h"
#include <vector>

using namespace std;


class Perf {
private:
	HashMap hash_perf;
	vector<Queue> per_core_perf;
	void data_struct_init();
	void timer_init();
	void timer_clear();


public:
	Perf() {}
	~Perf() {}
	void put_sample(sampleD);
	void start();
	void stop();
	void print() {};
	void print_core_buf();
};

extern Perf put_sample(sampleD);
#endif
