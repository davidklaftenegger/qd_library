#include<array>
#include<iostream>
#include<thread>
#include "cs.hpp"
#include "qd.hpp"

static qdlock lock;

void call_cs() {
	for(int i = 0; i < 100000000/THREADS; i++) {
		lock.DELEGATE_N(cs);
	}
}

void barrier() {
}

int main() {
	std::cout << THREADS << " threads / QD locking\n";
	cs_init();
	std::array<std::thread, THREADS> ts;
	for(auto& t : ts) {
		t = std::thread(&call_cs);
	}
	for(auto& t : ts) {
		t.join();
	}
	auto b = lock.DELEGATE_F(&barrier);
	b.wait();
	cs_finish();
}

