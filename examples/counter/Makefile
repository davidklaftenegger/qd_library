CXX=clang++
CXXFLAGS=-O2 -std=c++11 -I../../
LDFLAGS=-O2 -std=c++11 -pthreads -L/usr/local/lib/ -lqd

.PHONY: qd_comparison atomic qd_only all clean

COMPNUMS = 5 50 500
#COMPNUMS = 1 2 4 8 16 32 64 128

ONLYNUMS = 1 2 3 4 5 6 7 8 9

qd_comparison: $(addprefix counter_qd,${COMPNUMS}) $(addprefix counter_pthreads,${COMPNUMS}) $(addprefix counter_std,${COMPNUMS}) #$(addprefix counter_mcsfutex,${COMPNUMS}) $(addprefix counter_futex,${COMPNUMS})

atomic: $(addprefix counter_atomic,${COMPNUMS})

qd_only: $(addprefix counter_qd,${ONLYNUMS})

all: qd_comparison atomic qd_only

NUMS = $(shell seq 1 500)
QD_OBJ       := $(addsuffix .o,$(addprefix counter_qd_,${NUMS}))
PTHREADS_OBJ := $(addsuffix .o,$(addprefix counter_pthreads_,${NUMS}))
STD_OBJ      := $(addsuffix .o,$(addprefix counter_std_,${NUMS}))
MCSFUTEX_OBJ := $(addsuffix .o,$(addprefix counter_mcsfutex_,${NUMS}))
FUTEX_OBJ    := $(addsuffix .o,$(addprefix counter_futex_,${NUMS}))
ATOMIC_OBJ   := $(addsuffix .o,$(addprefix counter_atomic_,${NUMS}))


.PRECIOUS: counter_qd_%.o
$(QD_OBJ) : counter_qd_%.o : counter_qd.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) -DTHREADS=$*

.PRECIOUS: counter_pthreads_%.o
$(PTHREADS_OBJ) : counter_pthreads_%.o : counter_pthreads.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) -DTHREADS=$*

.PRECIOUS: counter_std_%.o
$(STD_OBJ) : counter_std_%.o : counter_std.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) -DTHREADS=$*

.PRECIOUS: counter_mcsfutex_%.o
$(MCSFUTEX_OBJ) : counter_mcsfutex_%.o : counter_mcsfutex.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) -DTHREADS=$*

.PRECIOUS: counter_futex_%.o
$(FUTEX_OBJ) : counter_futex_%.o : counter_futex.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) -DTHREADS=$*

.PRECIOUS: counter_atomic_%.o
$(ATOMIC_OBJ) : counter_atomic_%.o : counter_atomic.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) -DTHREADS=$*

cs.o cs_atomic.o: %.o : %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS)

counter_qd%: counter_qd_%.o cs.o
	$(CXX) -o $@ $^ $(LDFLAGS)
counter_pthreads%: counter_pthreads_%.o cs.o
	$(CXX) -o $@ $^ $(LDFLAGS)
counter_std%: counter_std_%.o cs.o
	$(CXX) -o $@ $^ $(LDFLAGS)
counter_mcsfutex%: counter_mcsfutex_%.o cs.o
	$(CXX) -o $@ $^ $(LDFLAGS)
counter_futex%: counter_futex_%.o cs.o
	$(CXX) -o $@ $^ $(LDFLAGS)
counter_atomic%: counter_atomic_%.o cs_atomic.o
	$(CXX) -o $@ $^ $(LDFLAGS)


clean:
	-@rm counter_pthreads[0-9]* \
		counter_std[0-9]* \
		counter_qd[0-9]* \
		counter_futex[0-9]* \
		counter_mcsfutex[0-9]* \
		counter_atomic[0-9]* \
		*.o 2>/dev/null || true
