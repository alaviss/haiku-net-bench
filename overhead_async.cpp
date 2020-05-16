#include <cerrno>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

#include <Errors.h>
#include <OS.h>

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
  nanotime_t start;
  nanotime_t recvTook;
  nanotime_t sendTook;
} job_data;

static status_t setNonBlock(int fd) {
  int flags = fcntl(fd, F_GETFL);
  if (flags == -1)
    return errno;
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) == -1)
    return errno;
  return 0;
}

static status_t runOrQueue(job_data &job, short &events) {
  switch (job.state) {
    case STATE_SEND:
      {
        if (job.sent < sizeof(REQUEST)) {
          if (job.sent == 0)
            job.start = system_time_nsecs();
          ssize_t sent = send(job.socket, &REQUEST[job.sent], sizeof(REQUEST) - job.sent, MSG_NOSIGNAL);
          if (sent == -1) {
            switch (errno) {
              case EINTR:
                return runOrQueue(job, events);
              case EWOULDBLOCK:
                events = POLLOUT;
                return B_WOULD_BLOCK;
              default:
                return errno;
            }
          } else {
            job.sent += sent;
            return runOrQueue(job, events);
          }
        } else if (job.sent > sizeof(REQUEST)) {
          return B_ERROR;
        } else {
          job.sendTook = system_time_nsecs() - job.start;
          job.state = STATE_RECV;
          return runOrQueue(job, events);
        }
      }
    case STATE_RECV:
      {
        if (job.recv == 0)
          job.start = system_time_nsecs();
        ssize_t read = recv(job.socket, job.buf, CHUNK, 0);
        if (read == -1) {
          switch (errno) {
            case EINTR:
              return runOrQueue(job, events);
            case EWOULDBLOCK:
              events = POLLIN;
              return B_WOULD_BLOCK;
            default:
              return errno;
          }
        } else if (read > 0) {
          job.recv += read;
          return runOrQueue(job, events);
        } else {
          job.recvTook = system_time_nsecs() - job.start;
          return B_OK;
        }
      }
  }
  return B_ERROR; /* unreachable */
}

int main() {
  const nanotime_t allStart = system_time_nsecs();
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = TEST_PORT;
  address.sin_addr.s_addr = TEST_ADDRESS;

  job_data jobs[NO_CONNECTIONS];
  memset(jobs, 0, sizeof(job_data) * NO_CONNECTIONS);
  struct pollfd fds[NO_CONNECTIONS];
  for (int i = 0; i < NO_CONNECTIONS; i++) {
    nanotime_t start = system_time_nsecs();
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1)
      panic(errno, "couldn't create a socket", return 1);
    panic(setNonBlock(sock), "couldn't set operation mode to non-blocking", return 1);
    if (connect(sock, (struct sockaddr*)&address, (socklen_t)sizeof(address)) == -1) {
      if (errno != EINTR && errno != EINPROGRESS) {
        panic(errno, "couldn't connect to server", return 1);
      }
    }
    printf("Socket created in %g us\n", (system_time_nsecs() - start) / USEC);

    start = system_time_nsecs();
    fds[i].fd = sock;
    fds[i].events = POLLOUT;

    jobs[i].id = i;
    jobs[i].socket = sock;
    jobs[i].buf = new char[CHUNK];

    printf("Job #%d setup completed in %g us\n", i,
           (system_time_nsecs() - start) / USEC);
  }

  int conn = NO_CONNECTIONS;
  while (conn > 0) {
    const int events = poll(fds, (nfds_t)NO_CONNECTIONS, -1);
    if (events == -1) {
      if (errno != EINTR && errno != EAGAIN)
        panic(errno, "poll failed", return 1);
    } else if (events > 0) {
      for (int i = 0, processed = 0; i < NO_CONNECTIONS && processed < events; i++) {
        if ((fds[i].revents & EVENT_RW) != 0) {
          processed++;
          const status_t ret = runOrQueue(jobs[i], fds[i].events);
          switch (ret) {
            case B_WOULD_BLOCK:
              {
                break;
              }
            case B_OK:
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
                printf("Job #%d failed with error: %s\n", jobs[i].id, strerror(ret));
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

  printf("All done in %g seconds\n", (system_time_nsecs() - allStart) / SEC);

  return 0;
}
