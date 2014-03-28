#include<array>
#include<iostream>
#include<pthread.h>
#include "cs.hpp"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* call_cs(void*) {
	for(int i = 0; i < 100000000/THREADS; i++) {
		pthread_mutex_lock(&mutex);
		cs();
		pthread_mutex_unlock(&mutex);
	}
	return nullptr;
}

int main() {
	std::cout << THREADS << " threads / pthreads locking\n";
	cs_init();
	std::array<pthread_t, THREADS> ts;
	for(auto& t : ts) {
		pthread_create(&t, NULL, call_cs, NULL);
	}
	for(auto& t : ts) {
		void* ignore;
		pthread_join(t, &ignore);
	}
	cs_finish();
}

