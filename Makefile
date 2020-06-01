ifeq ($(shell uname),Haiku)
  LIBS = -Wl,--as-needed -lnetwork
else
  LIBS = -Wl,--as-needed -pthread
endif
CXX := g++
CXXFLAGS := -O3 -Wall
BENCHS = overhead_threaded overhead_async
TOOLS = memory_monitor
SCHEDULING_RECORDER := scheduling_recorder
PAYLOAD_SIZE := 150 # MiB
NO_CONNECTIONS := 100

%: %.cpp
	$(CXX) $(CXXFLAGS) -DNO_CONNECTIONS=$(NO_CONNECTIONS) -o $@ $(filter-out %.h,$^) $(LIBS)

.PHONY = all clean profile record-scheduling setup-lighttpd

all: $(BENCHS) $(TOOLS)

overhead_async: overhead_async.cpp timers.cpp common.h timers.h
overhead_threaded: overhead_threaded.cpp timers.cpp common.h timers.h

ifeq ($(shell uname),Haiku)
memory_monitor: memory_monitor.cpp common.h
else
memory_monitor: memory_monitor.nim
	nim c -d:release $^
endif

benchmark: all
	./bench-overhead.sh

clean:
	rm -f $(BENCHS) $(TOOLS)
	rm -f lighttpd.conf
	rm -f testpayload

profile: all
	profile -o profile_threaded.log -a -c -C -f ./overhead_threaded
	profile -o profile_async.log -a -c -C -f ./overhead_async

record-scheduling: all
	$(SCHEDULING_RECORDER) sched_threaded.bin ./overhead_threaded
	$(SCHEDULING_RECORDER) sched_async.bin ./overhead_async

setup-lighttpd: lighttpd.conf.in
	sed "s|@PATH@|$(PWD)|" lighttpd.conf.in > lighttpd.conf
	dd if=/dev/urandom of=testpayload bs=1M count=$(PAYLOAD_SIZE)
	@echo "lighttpd configuration generated, to use, run: lighttpd -f $(PWD)/lighttpd.conf"
