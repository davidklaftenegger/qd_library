#include "mcs_futex_lock.hpp"

thread_local std::map<mcs_futex_lock*, mcs_node> mcs_futex_lock::mcs_node_store;
