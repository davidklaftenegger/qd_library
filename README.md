C++ QD Library
==============
[![Build Status](https://travis-ci.org/davidklaftenegger/qd_library.svg?branch=master)](https://travis-ci.org/davidklaftenegger/qd_library)

This is the C++ QD library, which provides implementations of
Queue Delegation (QD) locks for C++11 programs.

It is distributed under the Simplified BSD License.
Details can be found in the LICENSE file.

QD locks allow programmers to *delegate* critical sections to a lock. If the
lock is currently free, this will use an efficient lock implementation.
However, if the lock is already in use, the critical section will be delegated
to the thread currently holding the lock, who becomes the *helper*. The helper
will continue to execute critical sections for other threads until either it
finds no more work, or some maximum number of executions has been reached.

This scheme results in drastically higher throughput than with traditional
locking algorithms that transfer execution and shared data between threads.

For more details on QD locks see [this website](https://www.it.uu.se/research/group/languages/software/qd_lock_lib).

Requirements
------------

This library makes use of [libnuma](http://oss.sgi.com/projects/libnuma/), available on linux.
For other operating systems support can be turned off, at potential performance costs for NUMA machines.
To install libnuma on debian/Ubuntu and derivatives, run
```
sudo apt-get install libnuma-dev
```
For gentoo, run
```
sudo emerge sys-process/numactl
```

The following compilers are currently supported:
 * GCC: g++ versions 4.9 -- 6
 * Clang: clang++ versions 3.5 -- 3.8

Installation
------------

```
mkdir build
cd build
cmake ../
make -j8
make test
make install
```

Usage
-----

To use this library, include `qd.hpp` in your programs.

Here is a minimal example for how to use them:

```
#include<iostream>
#include<qd.hpp>
int main() {
	int counter = 0;
	qdlock lock;
	std::thread threads[4];
	for(auto& t : threads) {
		t = std::thread(
			[&lock, &counter]() {
				lock.delegate_n([&counter]() { counter++; });
			}
		);
	}
	for(auto& t : threads) {
	  t.join();
	}
	std::cout << "The counter value is " << counter << std::endl;
}
```

To compile and run a program it may be required to add `/usr/local/include/` to your include path and `/usr/local/lib` to your library path.
Compiling and running should then work using
```
g++ -I/usr/local/include/qd/ -L/usr/local/lib/ myprogram.cpp -pthread -lqd -o myprogram
./myprogram
```

Advanced Configuration
----------------------

If required, you can adjust some settings in CMake:
```
cmake -DQD_DEBUG=OFF                     \
      -DQD_TESTS=ON                      \
      -DQD_USE_LIBNUMA=ON                \
      -DCMAKE_INSTALL_PREFIX=/usr/local  \
      ../
```

C library
---------

For a library for C code, please see https://github.com/kjellwinblad/qd_lock_lib
