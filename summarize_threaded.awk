@namespace "overhead_threaded"

/Socket/ {
  jobCount++
  totalSocketCreateTime += $4
}

/setup completed/ {
  totalThreadStartTime += $6
}

/finished/ {
  bytes += $4
  totalRecvTime += $8
  totalAverageSpeed += $11
}

/All done/ {
  runs++
  totalTime += $4
}

END {
  totalDownloaded = bytes / 1024 / 1024
  printf "Total job count: %d\n", jobCount
  printf "Total successful runs: %d\n", runs
  printf "Total data fetched: %g MiB\n", totalDownloaded
  printf "Data fetched per job: %g MiB\n", totalDownloaded / jobCount
  printf "Average socket creation time: %g us\n", totalSocketCreateTime / jobCount
  printf "Average job setup time: %g us\n", totalThreadStartTime / jobCount
  printf "Average time spent receiving data: %g s\n", totalRecvTime / jobCount
  printf "Average download speed across all jobs: %g MiB/s\n", totalAverageSpeed / jobCount
  printf "Average run time: %g s\n", totalTime / runs
}
