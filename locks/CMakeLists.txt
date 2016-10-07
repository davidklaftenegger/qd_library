# put together all source files 
set(lock_sources mcs_lock.cpp mcs_futex_lock.cpp)

add_library(qd_locks ${lock_sources})
