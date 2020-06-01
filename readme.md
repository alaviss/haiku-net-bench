### Benchmarks to assess throughput and possible overhead associated with different approaches to networking I/O on Haiku

To compile, run:

    make [NO_CONNECTIONS=number of concurrent connections]

On Linux you will need the Nim compiler installed to compile the memory monitor.

The benchmark can then be run with:

    ./bench-overhead.sh [IPv4 address] [Port]

The environment variable `ITERATIONS` can be used to control how many iterations
of the benchmark should be run.

To generate kernel profiling data (note: will run the benchmark, requires the webserver to be setup):

    make profile # Will generate profile_*.log

    # Generates sched_*.bin
    make SCHEDULING_RECORDER=/path/to/scheduling_recorder record-scheduling

To generate `lighttpd.conf` and the data payload used for testing:

    make setup-lighttpd

If you want to use your own webserver, host one on `127.0.0.1:8000`. The payload will be requested at `/testpayload`.

Eventually more benchmarks and tools to query their data will be added.

#### Numbers

All of the tests are run with `NO_CONNECTIONS=100`, `TEST_PAYLOAD=200` and
`ITERATIONS=10`.

##### Over loopback (`127.0.0.1`)

                            Setup                              | Socket creation time | Job setup time |          Download           |  Run time  |  Average memory consumption  |
-------------------------------------------------------------- | -------------------- | -------------- | --------------------------- | ---------- | ---------------------------- |
Haiku (x86\_64, hrev54280, `KDEBUG_LEVEL 0`) Threaded          |      2.40832 us      |   224.159 us   |  39.9529 s @ 5.02905 MiB/s  |  42.043 s  |          7.01697 MiB         |
Haiku (x86\_64, hrev54280, `KDEBUG_LEVEL 0`) `poll()`-based    |      1.5305 us       |   47.7962 us   |  42.1472 s @ 4.75248 MiB/s  |  44.2072 s |          6.92944 MiB         |
Haiku (x86\_gcc2h, hrev54280, `KDEBUG_LEVEL 0`) Threaded       |      2.73067 us      |   211.687 us   |  45.4512 s @ 4.40853 MiB/s  |  47.7862 s |          6.892 MiB           |
Haiku (x86\_gcc2h, hrev54280, `KDEBUG_LEVEL 0`) `poll()`-based |      2.10034 us      |   58.2764 us   |  47.5437 s @ 4.23191 MiB/s  |  50.9748 s |          6.95322 MiB         |
Linux (x86\_64, 5.6.13) Threaded                               |      133.321 us      |   17.157 us    |  2.1227 s @ 94.2261 MiB/s   |  2.13348 s |          33.5141 MiB†        |
Linux (x86\_64, 5.6.13) `poll()-based`                         |      17.157 us       |   8.61118 us   |  2.37334 s @ 88.4764 MiB/s  |  2.79207 s |          306.113 MiB         |

† While the real RAM usage for "Linux threaded" might seem to be modest, the amount of virtual memory used was peaking 8GB.

##### Over QEMU net

                            Setup                              | Socket creation time | Job setup time |          Download           |  Run time  |  Average memory consumption  |
-------------------------------------------------------------- | -------------------- | -------------- | --------------------------- | ---------- | ---------------------------- |
Haiku (x86\_64, hrev54280, `KDEBUG_LEVEL 0`) Threaded          |      6.69758 us      |   151.929 us   |  184.726 s @ 1.11372 MiB/s  |  185.003 s |          6.37533 MiB         |
Haiku (x86\_64, hrev54280, `KDEBUG_LEVEL 0`) `poll()`-based    |      2.46532 us      |   20.209 us    |  122.542 s @ 1.75023 MiB/s  |  124.668 s |          6.94922 MiB         |
Haiku (x86\_gcc2h, hrev54280, `KDEBUG_LEVEL 0`) Threaded       |      34.1071 us      |   196.191 us   |  225.919 s @ 0.90875 MiB/s  |  226.919 s |          6.02247 MiB         |
Haiku (x86\_gcc2h, hrev54280, `KDEBUG_LEVEL 0`) `poll()`-based |      19.3235 us      |   19.3235 us   |  159.893 s @ 1.34646 MiB/s  |  159.4 s   |          6.1331 MiB          |

