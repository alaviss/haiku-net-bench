#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

#ifdef __HAIKU__
# include <OS.h>
#else
# include <pthread.h>
#endif

#include "common.h"
#include "timers.h"

typedef struct worker_data {
  int err;
  int socket;
#ifdef __HAIKU__
  thread_id id;
#else
  int id;
  pthread_t thread;
#endif
  size_t recv;
  int64_t recvTook;
  int64_t sendTook;
} worker_data;

static struct sockaddr_in address;

static void *worker(void *_data) {
  worker_data *data = (worker_data*)_data;
  if (data == NULL)
    return NULL;

  const int socket = data->socket;
  if (connect(socket, (struct sockaddr*)&address, (socklen_t)sizeof(address)) == -1) {
    if (errno == EINTR) {
      struct pollfd fds;
      memset(&fds, 0, sizeof(fds));
      fds.fd = socket;
      fds.events = POLLOUT;
      if (poll(&fds, 1, -1) == -1) {
        data->err = errno;
        return NULL;
      }
    } else {
      data->err = errno;
      return NULL;
    }
  }

  int64_t start = getMonoTime();
  size_t sent = 0;
  while (sent < sizeof(REQUEST)) {
    ssize_t ret = send(socket, &REQUEST[sent], sizeof(REQUEST) - sent, MSG_NOSIGNAL);
    if (ret == -1) {
      if (errno != EINTR) {
        return NULL;
      }
    } else {
      sent += ret;
    }
  }

  data->sendTook = getMonoTime() - start;

  start = getMonoTime();
  char *buf = new char[CHUNK];
  while (true) {
    ssize_t read = recv(socket, buf, CHUNK, 0);
    if (read == -1) {
      if (errno == EINTR)
        continue;
      data->err = errno;
      delete buf;
      return NULL;
    }
    if (read == 0)
      break;

    data->recv += read;
  }

  data->recvTook = getMonoTime() - start;
  delete buf;
  return NULL;
}

#ifdef __HAIKU__
static status_t haiku_worker(void *_data) {
  worker(_data);
  return B_OK;
}
#endif

int main(int argc, char *argv[]) {
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = argc > 2 ? htons((short)atoi(argv[2])) : TEST_PORT;
  address.sin_addr.s_addr = argc > 1 ? inet_addr(argv[1]) : TEST_ADDRESS;

  const int64_t allStart = getMonoTime();
  worker_data workers[NO_CONNECTIONS];
  memset(workers, 0, sizeof(worker_data) * NO_CONNECTIONS);
  for (int i = 0; i < NO_CONNECTIONS; i++) {
    int64_t start = getMonoTime();
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1)
      panic(errno, "couldn't create a socket", return 1);
    printf("Socket created in %g us\n", (getMonoTime() - start) / USEC);

    start = getMonoTime();
    workers[i].socket = sock;
#ifdef __HAIKU__
    workers[i].id = spawn_thread(haiku_worker, "benchmark worker", B_NORMAL_PRIORITY, &workers[i]);
    if (workers[i].id < 0) {
      errno = workers[i].id;
      panic(true, "couldn't spawn thread", return 1);
    }

    errno = resume_thread(workers[i].id);
    panic(errno != B_OK, "couldn't resume thread", return 1);
#else
    workers[i].id = i;
    errno = pthread_create(&workers[i].thread, NULL, worker, &workers[i]);
    panic(errno != 0, "couldn't spawn thread", return 1);
#endif
    printf("Job #%d setup completed in %g us\n", workers[i].id, (getMonoTime() - start) / USEC);
  }

  for (int i = 0; i < NO_CONNECTIONS; i++) {
#ifdef __HAIKU__
    status_t thread_status;
    wait_for_thread(workers[i].id, &thread_status);
#else
    errno = pthread_join(workers[i].thread, NULL);
    panic(errno != 0, "joining thread failed", return 1);
#endif
    if (workers[i].err == 0) {
      const double recvTimeInSecs = workers[i].recvTook / SEC;
      const double sendTimeInSecs = workers[i].sendTook / SEC;
      printf("Worker #%d finished: %zd bytes received in %g seconds (avg. %g MiB/s), "
             "%zd bytes sent in %g seconds (avg. %g B/s)\n",
             workers[i].id, workers[i].recv, recvTimeInSecs, workers[i].recv / MiB / recvTimeInSecs,
             sizeof(REQUEST), sendTimeInSecs, sizeof(REQUEST) / sendTimeInSecs);
    } else {
      printf("Worker #%d finished with error: %s\n", workers[i].id, strerror(workers[i].err));
    }
  }

  printf("All done in %g seconds\n", (getMonoTime() - allStart) / SEC);

  return 0;
}
