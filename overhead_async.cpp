#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

#include "timers.h"

#ifdef __HAIKU__
# include <OS.h>
#endif

#include "common.h"

enum job_state {
  STATE_SEND,
  STATE_RECV
};

typedef struct job_data {
  int id;
  enum job_state state;
  int socket;
  char *buf;
  size_t sent;
  size_t recv;
  uint64_t start;
  uint64_t recvTook;
  uint64_t sendTook;
} job_data;

static int setNonBlock(int fd) {
  int flags = fcntl(fd, F_GETFL);
  if (flags == -1)
    return -1;
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) == -1)
    return -1;
  return 0;
}

static int runOrQueue(job_data &job, short &events) {
  switch (job.state) {
    case STATE_SEND:
      {
        if (job.sent < sizeof(REQUEST)) {
          if (job.sent == 0)
            job.start = getMonoTime();
          ssize_t sent = send(job.socket, &REQUEST[job.sent], sizeof(REQUEST) - job.sent, MSG_NOSIGNAL);
          if (sent == -1) {
            switch (errno) {
              case EINTR:
                return runOrQueue(job, events);
              case EWOULDBLOCK:
                events = POLLOUT;
                return 1;
              default:
                return -1;
            }
          } else {
            job.sent += sent;
            return runOrQueue(job, events);
          }
        } else if (job.sent > sizeof(REQUEST)) {
          return -1;
        } else {
          job.sendTook = getMonoTime() - job.start;
          job.state = STATE_RECV;
          return runOrQueue(job, events);
        }
      }
    case STATE_RECV:
      {
        if (job.recv == 0)
          job.start = getMonoTime();
        ssize_t read = recv(job.socket, job.buf, CHUNK, 0);
        if (read == -1) {
          switch (errno) {
            case EINTR:
              return runOrQueue(job, events);
            case EWOULDBLOCK:
              events = POLLIN;
              return 1;
            default:
              return -1;
          }
        } else if (read > 0) {
          job.recv += read;
          return runOrQueue(job, events);
        } else {
          job.recvTook = getMonoTime() - job.start;
          return 0;
        }
      }
  }
  return -1; /* unreachable */
}

int main(int argc, char *argv[]) {
  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = argc > 2 ? htons((short)atoi(argv[2])) : TEST_PORT;
  address.sin_addr.s_addr = argc > 1 ? inet_addr(argv[1]) : TEST_ADDRESS;

  const uint64_t allStart = getMonoTime();
  job_data jobs[NO_CONNECTIONS];
  memset(jobs, 0, sizeof(job_data) * NO_CONNECTIONS);
  struct pollfd fds[NO_CONNECTIONS];
  for (int i = 0; i < NO_CONNECTIONS; i++) {
    uint64_t start = getMonoTime();
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    panic(sock == -1, "couldn't create a socket", return 1);
    panic(setNonBlock(sock) == -1, "couldn't set operation mode to non-blocking", return 1);
    printf("Socket created in %g us\n", (getMonoTime() - start) / USEC);

    start = getMonoTime();
    fds[i].fd = sock;
    fds[i].events = POLLOUT;

    jobs[i].id = i;
    jobs[i].socket = sock;
    jobs[i].buf = new char[CHUNK];

    if (connect(sock, (struct sockaddr*)&address, (socklen_t)sizeof(address)) == -1) {
      panic(errno != EINTR && errno != EINPROGRESS, "couldn't connect to server", return 1);
    }

    printf("Job #%d setup completed in %g us\n", i,
           (getMonoTime() - start) / USEC);
  }

  int conn = NO_CONNECTIONS;
  while (conn > 0) {
    const int events = poll(fds, (nfds_t)NO_CONNECTIONS, -1);
    if (events == -1) {
      panic(errno != EINTR && errno != EAGAIN, "poll failed", return 1);
    } else if (events > 0) {
      for (int i = 0, processed = 0; i < NO_CONNECTIONS && processed < events; i++) {
        if ((fds[i].revents & EVENT_RW) != 0) {
          processed++;
          switch (runOrQueue(jobs[i], fds[i].events)) {
            case 1:
              {
                break;
              }
            case 0:
              {
                const double recvTimeInSecs = jobs[i].recvTook / SEC;
                const double sendTimeInSecs = jobs[i].sendTook / SEC;
                printf("Job #%d finished: %zd bytes received in %g seconds (avg. %g MiB/s), "
                       "%zd bytes sent in %g seconds (avg. %g B/s)\n",
                       jobs[i].id, jobs[i].recv, recvTimeInSecs, jobs[i].recv / MiB / recvTimeInSecs,
                       jobs[i].sent, sendTimeInSecs, jobs[i].sent / sendTimeInSecs);
                fds[i].fd = -1;
                delete jobs[i].buf;
                conn--;
                break;
              }
            default:
              {
                printf("Job #%d failed with error: %s\n", jobs[i].id, strerror(errno));
                fds[i].fd = -1;
                delete jobs[i].buf;
                conn--;
                break;
              }
          }
        } else if ((fds[i].revents & EVENT_INVALID) != 0) {
          processed++;
          fds[i].fd = -1;
          conn--;
        }
      }
    }
  }

  printf("All done in %g seconds\n", (getMonoTime() - allStart) / SEC);

  return 0;
}
