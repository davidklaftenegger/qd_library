#include<array>
#include<iostream>
#include<mutex>
#include<thread>
#include "cs.hpp"
#include "../../locks/mcs_futex_lock.hpp"

static mcs_futex_lock lock;

void call_cs() {
	for(int i = 0; i < 100000000/THREADS; i++) {
		std::lock_guard<mcs_futex_lock> l(lock);
		cs();
	}
}

int main() {
	std::cout << THREADS << " threads / mcs_futex locking\n";
	cs_init();
	std::array<std::thread, THREADS> ts;
	for(auto& t : ts) {
		t = std::thread(&call_cs);
	}
	for(auto& t : ts) {
		t.join();
	}
	cs_finish();
}

