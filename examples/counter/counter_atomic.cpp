#include<array>
#include<iostream>
#include<mutex>
#include<thread>
#include "cs.hpp"

void call_cs() {
	for(int i = 0; i < 100000000/THREADS; i++) {
		cs();
	}
}

int main() {
	std::cout << THREADS << " threads / no locking (using atomic instruction)\n";
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

