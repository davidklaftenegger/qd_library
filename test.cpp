#include<iostream>
#include "qd.hpp"

int shared;
int foo(int i) {
	shared += 2;
//	std::cout << "Hello World! " << i << "\n";
	return 23;
}

void bar(int i) {
	shared++;
//	std::cout << "Hello World! " << i << "\n";
}

mrqdlock qdl;

void f1() {
	typedef std::future<int> future;
	std::array<future, 1024*4> fs;
	for(future& f : fs) {
		f = qdl.delegate(foo, 42);
	}
}

void f2() {
	typedef std::future<void> future;
	std::array<future, 1024*4> fs;
	for(future& f : fs) {
		f = qdl.delegate(bar, 23);
	}
}


int main() {
	shared = 0;
	std::array<std::thread, 4> t1;
	std::array<std::thread, 4> t2;
	for(std::thread& t : t1) {
		t = std::thread(f1);
	}
	for(std::thread& t : t2) {
		t = std::thread(f2);
	}
	
	for(std::thread& t : t1) {
		t.join();
	}
	for(std::thread& t : t2) {
		t.join();
	}
	
	return 0;
}
