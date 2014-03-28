CXX=clang++
LD=clang++
CXXFLAGS=-O2 -std=c++11 -I../../
LDFLAGS=-O2 -std=c++11 -pthreads

all: counter_pthreads5 counter_std5 counter_qd5 counter_pthreads50 counter_std50 counter_qd50 counter_pthreads500 counter_std500 counter_qd500

%_5.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) -DTHREADS=5

%_50.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) -DTHREADS=50

%_500.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) -DTHREADS=500

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS)

counter_pthreads5: counter_pthreads_5.o cs.o
	$(LD) -o $@ $^ $(LDFLAGS)
counter_std5: counter_std_5.o cs.o
	$(LD) -o $@ $^ $(LDFLAGS)
counter_qd5: counter_qd_5.o cs.o
	$(LD) -o $@ $^ $(LDFLAGS)

counter_pthreads50: counter_pthreads_50.o cs.o
	$(LD) -o $@ $^ $(LDFLAGS)
counter_std50: counter_std_50.o cs.o
	$(LD) -o $@ $^ $(LDFLAGS)
counter_qd50: counter_qd_50.o cs.o
	$(LD) -o $@ $^ $(LDFLAGS)

counter_pthreads500: counter_pthreads_500.o cs.o
	$(LD) -o $@ $^ $(LDFLAGS)
counter_std500: counter_std_500.o cs.o
	$(LD) -o $@ $^ $(LDFLAGS)
counter_qd500: counter_qd_500.o cs.o
	$(LD) -o $@ $^ $(LDFLAGS)

clean:
	-@rm counter_pthreads5 counter_pthreads50 counter_pthreads500 counter_std5 counter_std50 counter_std500 counter_qd5 counter_qd50 counter_qd500 *.o 2>/dev/null || true
