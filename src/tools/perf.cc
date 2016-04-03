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
      per_core_perf.push_back(Queue());
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
	Timeout::sleep(Clock::now() + 100);
//	KOUT::outl("# of samples ", hash_perf->numberOfEntry());

	while ( hash_perf->numberOfEntry() <= TOTAL_SAMPLES) {
		//KOUT::outl("# of samples ", hash_perf->numberOfEntry());
		if (is_data_ready()) {
			for (unsigned int i=0; i < per_core_perf.size() ;i++) {
				for (int j=0; j < per_core_perf[i].qsize() ; j++) {			 
					hash_perf->put(per_core_perf[i].view(j));
				}		
				per_core_perf[i].data_read();
			}
			data_ready_flag = false;
		}
		Timeout::sleep(Clock::now() + 100);
	}	
	stop();

}

void Perf::put_sample(sampleD sample) {
	//KOUT::outl("pushing key in cpu q ", sample.address, " from cpu ", sample.cpuID);
	//KOUT::outl("cpu q size ", per_core_perf[sample.cpuID].qsize());
	per_core_perf[sample.cpuID].push(sample);
	if (per_core_perf[sample.cpuID].qsize() >= QLIMIT) {
		data_ready_flag = true;
	}
}

void Perf::stop() {
	timer_clear();
}

bool Perf::is_data_ready() {
	return data_ready_flag;
}

void Perf::print() {
	hash_perf->topTen();
}


void Perf::print_core_buf() {
	for (unsigned int i=0; i < per_core_perf.size() ; i++ ) {
		KOUT::outl("Core: ", i);
		for (int j=0; j < per_core_perf[i].qsize() ; j++) {
			//KOUT::outl("size of the core ", per_core_perf[i].qsize());
			KOUT::outl("addr ", per_core_perf[i].view(j));
		}
	}
}

