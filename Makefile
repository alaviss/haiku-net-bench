DEPS = common.h
LIBS = -lnetwork
CXX := g++
CXXFLAGS := -O3
BENCHS = overhead_threaded overhead_async
SCHEDULING_RECORDER := scheduling_recorder
PAYLOAD_SIZE := 150 # MiB
NO_CONNECTIONS := 100

%: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -DNO_CONNECTIONS=$(NO_CONNECTIONS) -o $@ $(filter-out %.h,$^) $(LIBS)

.PHONY = all clean profile record-scheduling setup-lighttpd

all: $(BENCHS)

clean:
	rm -f $(BENCHS)
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
	: "lighttpd configuration generated, to use, run: lighttpd -f $(PWD)/lighttpd.conf"
