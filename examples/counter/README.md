Shared Counter Example
======================
Introduction
------------
In this example we evaluate various ways of implementing a program that
launches N threads, which each execute a critical section X times,
such that the total number of critical section executions equals:
X/N - 100,000,000

Building
--------
make
builds a version for 5, 50 and 500 threads for each of the locking schemes:
 a) pthreads uses pthreads directly: `counter_pthreads{5,50,500}`
 b) std uses `std::mutex` and `std::lock_guard` for locking: `counter_std{5,50,500}`
 c) qd uses QD locking as provided by this library: `counter_qd{5,50,500}`

Comparison with atomic instructions
-----------------------------------
make atomics
additionally builds versions using atomic instructions instead of locks.

Running
-------
You can use the following command to view output including runtimes:
    for counter in *5 *50 *500; do ./$counter; echo; done


Results
-------
**This is not a benchmark:**
These example programs do not try to minimize variation of results. E.g., the
worker threads start working immediately, not when all threads are spawned.
The workload in this example also is not representative of any real world load.
However, the numbers below show how much time is spent on synchronization in
different implementations.

### Intel(R) Core(TM) i7-3770 CPU @ 3.40GHz ###
4 cores with 2 threads each
```
            |   total number of threads   |
            |    5    |   50    |   500   |
            | ------- | ------- | ------- |
 pthreads   | 17229ms | 21879ms | 18162ms |
 std::mutex | 16644ms | 24020ms | 21734ms |
 QD locking |  3422ms |  4037ms |  4878ms |
 atomic cs  |  1689ms |  1830ms |  1833ms | 
```

Files
-----
* `counter_pthreads.cpp` - pthreads-only implementation
* `counter_std.cpp` - std::mutex based implementation
* `counter_qd.cpp` - using the qd locks provided by this library
* `cs.hpp` - interface definition for the critical section
* `cs.cpp` - the critical section implementation (not thread-safe)
* `cs_atomic.cpp` - thread-safe variant of cs.cpp using atomic instructions
