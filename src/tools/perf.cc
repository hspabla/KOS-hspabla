#include "perf.h"

void set_timer(){
  KOUT::outl("apicid from perf_init ", LocalProcessor::getApicID());
  LocalProcessor::setApicTimer();
}


void clear_timer(){
  KOUT::outl("apicid from perf_init ", LocalProcessor::getApicID());
  LocalProcessor::clearApicTimer();
}


void Perf::data_struct_init(){
    for (mword i = 0; i < Machine::getProcessorCount(); i++) {
      Queue temp;
      per_core_perf.push_back(temp);
    }
}

void Perf::timer_init(){
	for (mword i = 0; i < Machine::getProcessorCount(); i++) {
    	Thread* t = Thread::create();
    	t->setScheduler(Machine::getProcessor(i).getScheduler())->setAffinity(true);
    	t->start((ptr_t)set_timer);
    }
}

void Perf::timer_clear(){
	for (mword i = 0; i < Machine::getProcessorCount(); i++) {
    	Thread* t = Thread::create();
    	t->setScheduler(Machine::getProcessor(i).getScheduler())->setAffinity(true);
    	t->start((ptr_t)clear_timer);
    }
}

void Perf::start() {
	data_struct_init();
	timer_init();
}

void Perf::put_sample(sampleD sample) {
	per_core_perf[sample.cpuID].push(sample);
}

void Perf::stop() {
	timer_clear();
}

void Perf::print_core_buf() {
	for (int i=0; i < per_core_perf.size() ; i++ ) {
		KOUT::outl("Core: ", i);
		for (int j=0; per_core_perf[i].qsize() ; j++) {
			KOUT::outl("addr ", per_core_perf[i].view(j));
		}
	}
}

