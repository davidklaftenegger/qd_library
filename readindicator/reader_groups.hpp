#ifndef qd_reader_groups_hpp
#define qd_reader_groups_hpp qd_reader_groups_hpp

#include "threadid.hpp"

template<int GROUPS>
class reader_groups {
	std::array<std::atomic<int>, GROUPS> counters;
	public:
		reader_groups() {
			for(int i = 0; i < GROUPS; i++) {
				counters[i] = 0;
			}
		}
		bool query() {
			for(std::atomic<int>& counter : counters)
				if(counter.load() > 0) return true;
			return false;
		}
		void arrive() {
			counters[thread_id % GROUPS] += 1;
		}
		void depart() {
			counters[thread_id % GROUPS] -= 1;
		}
};

#endif /* qd_reader_groups_hpp */
