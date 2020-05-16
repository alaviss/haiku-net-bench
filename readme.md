### Benchmarks to assess throughput and possible overhead associated with different approaches to networking I/O on Haiku

To compile, run:

    make [NO_CONNECTIONS=number of concurrent connections]

To generate kernel profiling data (note: will run the benchmark, requires the webserver to be setup):

    make profile # Will generate profile_*.log

    # Generates sched_*.bin
    make SCHEDULING_RECORDER=/path/to/scheduling_recorder record-scheduling

To generate `lighttpd.conf` and the data payload used for testing:

    make setup-lighttpd

If you want to use your own webserver, host one on `127.0.0.1:8000`. The payload will be requested at `/testpayload`.

Eventually more benchmarks and tools to query their data will be added.
