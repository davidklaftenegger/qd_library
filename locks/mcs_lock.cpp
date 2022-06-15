#include "mcs_lock.hpp"

using namespace qd::locks;
thread_local std::map<mcs_lock*, mcs_node2> mcs_lock::mcs_node_store;
