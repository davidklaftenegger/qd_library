C++ QD Locking Library
======================
[![Build Status](https://github.com/davidklaftenegger/qd_library/workflows/CI/badge.svg)](https://github.com/davidklaftenegger/qd_library/actions)

This is the C++ QD Locking library, which provides implementations of Queue Delegation
(QD) locks for C++11 programs as described in the following two publications:

 1. David Klaftenegger, Konstantinos Sagonas, and Kjell Winblad.
    [Queue Delegation Locking](https://ieeexplore.ieee.org/document/8093701).
    In IEEE Transactions on Parallel and Distributed Systems, 29(3):687-704,
    March 2018. IEEE.
    DOI: [10.1109/TPDS.2017.2767046](https://doi.org/10.1109/TPDS.2017.2767046)

 2. David Klaftenegger, Konstantinos Sagonas, and Kjell Winblad.
    [Delegation Locking Libraries for Improved Performance of Multithreaded
     Programs](https://link.springer.com/chapter/10.1007/978-3-319-09873-9_48).
    In Euro-Par 2014, Proceedings of the 20th International Conference,
    pp. 572-583, Volume 8632 in LNCS, August 2014. Springer.
    DOI: [10.1007/978-3-319-09873-9_48](https://doi.org/10.1007/978-3-319-09873-9_48)

The QD Locking library is distributed under the Simplified BSD License.
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

This library makes use of [libnuma](http://oss.sgi.com/projects/libnuma/), available on Linux.
For other operating systems support can be turned off, at potential performance costs for NUMA machines.
To install libnuma on Debian/Ubuntu and derivatives, run
```bash
sudo apt-get install libnuma-dev
```
For gentoo, run
```bash
sudo emerge sys-process/numactl
```

The following compilers are currently supported:
 * GCC: g++ versions 4.9 -- 6
 * Clang: clang++ versions 3.8 -- 3.8

Installation
------------

```bash
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

```c++
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
```bash
g++ -I/usr/local/include/qd/ -L/usr/local/lib/ -Wl,-rpath=/usr/local/lib/ myprogram.cpp -pthread -lqd -o myprogram
./myprogram
```

Advanced Configuration
----------------------

If required, you can adjust some settings in CMake:
```bash
cmake -DQD_DEBUG=OFF                     \
      -DQD_TESTS=ON                      \
      -DQD_USE_LIBNUMA=ON                \
      -DCMAKE_INSTALL_PREFIX=/usr/local  \
      ../
```

C library
---------

For a library for C code, please see https://github.com/kjellwinblad/qd_lock_lib
