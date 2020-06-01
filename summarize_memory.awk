@namespace "memory_monitor"

/Memory usage statistics/ {
  runs++
  samples += substr($4, 2)
  min += $7
  avg += $10
  max += $13
}

END {
  printf "Memory usage statistics (averaged over %d runs, %d samples): min: %g MiB avg: %g MiB max: %g MiB\n", runs, samples, min / runs, avg / runs, max / runs
}
