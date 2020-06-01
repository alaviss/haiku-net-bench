#!/usr/bin/env bash

: ${ITERATIONS:=10}

for bench in overhead_threaded overhead_async; do
  echo -e "\e[1m\e[36m::\e[39m Running $ITERATIONS iterations of $bench\e[0m"
  (
    for i in $(seq 1 "$ITERATIONS"); do
      ./memory_monitor "./$bench" "$@"
    done
  ) | awk -f "${bench/overhead/summarize}.awk" -f summarize_memory.awk
done
