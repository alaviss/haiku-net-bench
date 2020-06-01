#include <cerrno>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __HAIKU__
# include <Errors.h>
# include <OS.h>
#endif

#include "common.h"

#define TICKS 100

void printUsage() {
  printf("memory_monitor <program> [args]\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printUsage();
    return 1;
  }

  pid_t child = fork();
  switch (child) {
    case -1:
      panic(errno, "couldn't fork()", return 1);
      break;
    case 0:
      execvp(argv[1], argv + 1);
      panic(errno, "couldn't start child process", return 127);
      break;
    default:
      break;
  }

  int64 totalRss = 0, lowestRss = 0, highestRss = 0;
  int collected = 0;
  thread_info thinfo;
  panic(get_thread_info(child, &thinfo), "couldn't obtain main child thread information", return 1);
  object_wait_info info = { child, B_OBJECT_TYPE_THREAD, 0 };
  while ((info.events & B_EVENT_INVALID) == 0) {
    area_info ainfo;
    ssize_t cookie = 0;
    int64 rss = 0;
    while (get_next_area_info(thinfo.team, &cookie, &ainfo) == B_OK) {
      rss += ainfo.ram_size;
    }
    collected++;
    totalRss += rss;
    if ((rss < lowestRss && rss > 0) || lowestRss == 0) lowestRss = rss;
    if (rss > highestRss) highestRss = rss;
    ssize_t selected = wait_for_objects_etc(&info, 1, B_RELATIVE_TIMEOUT, TICKS);
    if (selected < 0 && selected != B_TIMED_OUT)
      panic(selected, "wait_for_objects failed", return 1);
  }
  int stat;
  while (true) {
    if (waitpid(child, &stat, 0) == -1) {
      if (errno != EINTR)
        panic(errno, "waitpid failed", return 1);
    } else {
      break;
    }
  }

  if (WEXITSTATUS(stat) != 0) {
    printf("Child process terminated with unclean exit code %d\n", WEXITSTATUS(stat));
    return WEXITSTATUS(stat);
  }

  printf("Memory usage statistics (%d samples): min: %g MiB avg: %g MiB max: %g MiB\n",
         collected, lowestRss / (double)MiB, totalRss / (double)collected / MiB, highestRss / (double)MiB);

  return 0;
}
