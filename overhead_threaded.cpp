#include <cerrno>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>

#include <Errors.h>
#include <OS.h>

#include "common.h"

typedef struct worker_data {
  int socket;
  thread_id tid;
  size_t recv;
  nanotime_t recvTook;
  nanotime_t sendTook;
} worker_data;

static status_t worker(void *_data) {
  worker_data *data = (worker_data*)_data;
  if (data == NULL)
    return B_BAD_VALUE;

  const int socket = data->socket;
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = TEST_PORT;
  address.sin_addr.s_addr = TEST_ADDRESS;

  if (connect(socket, (struct sockaddr*)&address, (socklen_t)sizeof(address)) == -1) {
    if (errno == EINTR) {
      object_wait_info conn = { socket, B_OBJECT_TYPE_FD, B_EVENT_WRITE };
      ssize_t events = wait_for_objects(&conn, 1);
      if (events < 0) {
        return events;
      } else if (events == 0) {
        return B_ERROR;
      } else if ((conn.events & B_EVENT_WRITE) == 0) {
          return B_ERROR;
      }
    } else {
      return errno;
    }
  }
  const nanotime_t startSend = system_time_nsecs();
  size_t sent = 0;
  while (sent < sizeof(REQUEST)) {
    ssize_t ret = send(socket, &REQUEST[sent], sizeof(REQUEST) - sent, MSG_NOSIGNAL);
    if (ret == -1) {
      if (errno != EINTR) {
        return errno;
      }
    } else {
      sent += ret;
    }
  }

  data->sendTook = system_time_nsecs() - startSend;

  const nanotime_t startRecv = system_time_nsecs();
  char *buf = new char[CHUNK];
  while (true) {
    ssize_t read = recv(socket, buf, CHUNK, 0);
    if (read == -1) {
      if (read == EINTR)
        continue;
      delete buf;
      return read;
    }
    if (read == 0)
      break;

    data->recv += read;
  }

  data->recvTook = system_time_nsecs() - startRecv;
  delete buf;
  return B_OK;
}

int main() {
  const nanotime_t allStart = system_time_nsecs();

  worker_data workers[NO_CONNECTIONS];
  memset(workers, 0, sizeof(worker_data) * NO_CONNECTIONS);
  for (int i = 0; i < NO_CONNECTIONS; i++) {
    nanotime_t start = system_time_nsecs();
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1)
      panic(errno, "couldn't create a socket", return 1);
    printf("Socket created in %g us\n", (system_time_nsecs() - start) / USEC);

    start = system_time_nsecs();
    workers[i].socket = sock;
    workers[i].tid = spawn_thread(worker, "benchmark worker", B_NORMAL_PRIORITY, &workers[i]);
    if (workers[i].tid < 0)
      panic(workers[i].tid, "couldn't spawn thread", return 1);

    panic(resume_thread(workers[i].tid), "couldn't resume thread", return 1);
    printf("Launched worker thread id %d in %g us\n", workers[i].tid,
           (system_time_nsecs() - start) / USEC);
  }

  for (int i = 0; i < NO_CONNECTIONS; i++) {
    status_t thread_status;
    wait_for_thread(workers[i].tid, &thread_status);
    if (thread_status == B_OK) {
      const double recvTimeInSecs = workers[i].recvTook / SEC;
      const double sendTimeInSecs = workers[i].sendTook / SEC;
      printf("Worker thread id %d finished: %zd bytes received in %g seconds (avg. %g MiB/s), "
             "%zd bytes sent in %g seconds (avg. %g B/s)\n",
             workers[i].tid, workers[i].recv, recvTimeInSecs, workers[i].recv / MiB / recvTimeInSecs,
             sizeof(REQUEST), sendTimeInSecs, sizeof(REQUEST) / sendTimeInSecs);
    } else {
      printf("Worker thread id %d finished with error: %s\n", workers[i].tid, strerror(thread_status));
    }
  }

  printf("All done in %g seconds\n", (system_time_nsecs() - allStart) / SEC);

  return 0;
}
