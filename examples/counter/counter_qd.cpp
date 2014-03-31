#include<array>
#include<iostream>
#include<thread>
#include "cs.hpp"
#include "qd.hpp"

static qdlock lock;

void call_cs() {
	for(int i = 0; i < 100000000/THREADS; i++) {
		/* DELEGATE_N does not wait for a result */
		lock.DELEGATE_N(cs);
	}
}

/* an empty function can be used to wait for previously delegated sections:
 * when the (empty) function is executed, everything preceding it is also done.
 */
void empty() {
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
	/* the empty function is used as a marker:
	 * all preceding sections are executed before it */
	auto b = lock.DELEGATE_F(&empty);
	b.wait(); /* wait for the call to empty() */

	cs_finish();
}

