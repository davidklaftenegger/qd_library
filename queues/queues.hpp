#ifndef qd_queues_queues_hpp
#define qd_queues_queues_hpp qd_queues_queues_hpp

namespace qd {
	/**
	 * @brief namespace for queue implementations
	 */
	namespace queues {
		/** @brief type alias for stored function type */
		using ftype = void(*)(char*);

		/** constants for current state of the queue */
		enum class status { OPEN=0, SUCCESS=0, FULL, CLOSED };

	} // namespace queues
} // namespace qd
#endif /* qd_queues_queues_hpp */
