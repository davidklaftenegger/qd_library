#include<iostream>
#include "qd.hpp"

int foo(int i) {
	std::cout << "Hello World! " << i << "\n";
	return 23;
}

void bar(int i) {
	std::cout << "Hello World! " << i << "\n";
}

qdlock qdl;

void f1() {
	typedef std::future<int> future;
	std::array<future, 1024*4> fs;
	for(future& f : fs) {
		f = qdl.delegate(std::function<int(int)>(foo), 42);
	}
}

void f2() {
	typedef std::future<void> future;
	std::array<future, 1024*4> fs;
	for(future& f : fs) {
		f = qdl.delegate(std::function<void(int)>(bar), 23);
	}
}


int main() {
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
