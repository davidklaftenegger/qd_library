#include<iostream>
#include "qd.hpp"

struct myInt {
	int* i;
	myInt() : i(new int(42)) {}
	myInt(int i) : i(new int(i)) {}
	myInt(const myInt& i) : i(new int(*i.i)) {}
	myInt(const myInt&& i) : i(new int(*i.i)) {}
	~myInt() { delete i; }
	void inc(myInt amount) {
		*i += *amount.i;
		std::cout << "ADDITION: " << *i << " =added= " << *amount.i << '\n';
	}
};

struct Fun {
	myInt operator()(myInt a, myInt b) {
		auto r = myInt(*a.i + *b.i);
		std::cout << *a.i << "+" << *b.i << " = " << *r.i << '\n';
		return r;
	}
};
int shared;
int foo(myInt i) {
	shared += 2;
//	std::cout << "Hello World! " << i << "\n";
	return 23;
}

void bar(myInt i) {
	shared++;
//	std::cout << "Hello World! " << i << "\n";
}

void wait() {
}

qdlock qdl;

void f1() {
	typedef std::future<int> future;
	std::array<future, 1024*128> fs;
	for(future& f : fs) {
		//f = qdl.delegate<decltype(foo), foo>(myInt(42));
		myInt m(42);
		f = qdl.DELEGATE_FUTURE(foo, m);
		//f = qdl.DELEGATE_FUTURE(foo, myInt(42));
	}
}

void f2() {
	typedef std::future<void> future;
	std::array<std::pair<myInt,Fun>, 1024*128> ms;
	Fun f;
	for(auto& m : ms) {
		qdl.DELEGATE_NOFUTURE(&myInt::inc, myInt(m.first), myInt(23));
		qdl.delegate_n(&myInt::inc, myInt(m.first), myInt(23));
		//qdl.delegate_nofuture(&foo, m.first);
		//qdl.DELEGATE_NOFUTURE(&foo, m.first);
	}
	for(auto& m : ms) {
		qdl.delegate_n(f, myInt(m.first), myInt(23));
	}
	auto bar = qdl.DELEGATE_FUTURE(wait);
	bar.wait();
	for(auto& m : ms) {
		std::cout << *m.first.i << '\n';
	}
	bar = qdl.DELEGATE_FUTURE(wait);
	bar.wait();
}


int main() {
	shared = 0;
	std::array<std::thread, 1> t1;
	std::array<std::thread, 1> t2;
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
