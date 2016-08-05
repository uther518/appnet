
#include "appnet_server.h"
#include "ae.h"
#include "anet.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

void initOnLoopStart(struct aeEventLoop *el) {}

int getPipeIndex(int connfd) {
  return (connfd % servG->worker_num) * servG->reactor_num +
         connfd % servG->reactor_num;
}

void initThreadOnLoopStart(struct aeEventLoop *el) {}

void onSignEvent(aeEventLoop *el, int fd, void *privdata, int mask) {}

void freeClient(appnetConnection *conn) {
  if (conn->connfd != -1) {
    conn->disable = 1;

    int reactor_id = conn->connfd % servG->reactor_num;
    aeEventLoop *el = servG->reactor_threads[reactor_id].reactor.event_loop;

    aeDeleteFileEvent(el, conn->connfd, AE_READABLE);
    aeDeleteFileEvent(el, conn->connfd, AE_WRITABLE);

    sdsfree(conn->send_buffer);
    sdsfree(conn->recv_buffer);

    servG->connect_num -= 1;
    close(conn->connfd);
  }
}

void onCloseByClient(aeEventLoop *el, void *privdata, appnetServer *serv,
                     appnetConnection *conn) {

  if (conn == NULL) {
    return;
  }

  appnetPipeData data = {0};
  data.type = PIPE_EVENT_CLOSE;
  data.connfd = conn->connfd;
  data.len = 0;

  int worker_id = servG->connlist[conn->connfd].worker_id;
  setPipeWritable(el, privdata, conn->connfd);

  /* append close event message to sendbuffer will send to worker */
  pthread_mutex_lock(&servG->workers[worker_id].w_mutex);

  servG->workers[worker_id].send_buffer = sdscatlen(
      servG->workers[worker_id].send_buffer, &data, PIPE_DATA_HEADER_LENG);

  pthread_mutex_unlock(&servG->workers[worker_id].w_mutex);

  freeClient(conn);
}

void onClientWritable(aeEventLoop *el, int connfd, void *privdata, int mask) {
  ssize_t nwritten;
  if (servG->connlist[connfd].disable == 1) {
    printf("Connect disable,fd=%d \n", connfd);
    return;
  }

  if (sdslen(servG->connlist[connfd].send_buffer) <= 0) {
    printf("send_buffer empty fd=%d \n", connfd);
    aeDeleteFileEvent(el, connfd, AE_WRITABLE);
    return;
  }

  nwritten = write(connfd, servG->connlist[connfd].send_buffer,
                   sdslen(servG->connlist[connfd].send_buffer));

  if (nwritten <= 0) {
    printf("I/O error writing to client: %s \n", strerror(errno));
    freeClient(&servG->connlist[connfd]);
    return;
  }

  sdsrange(servG->connlist[connfd].send_buffer, nwritten, -1);

  if (sdslen(servG->connlist[connfd].send_buffer) == 0) {
    aeDeleteFileEvent(el, connfd, AE_WRITABLE);

    if (servG->connlist[connfd].disable == 2 ||
        servG->connlist[connfd].proto_type == HTTP) {
      /* if connfd is http or disabled, free and close it */
      freeClient(&servG->connlist[connfd]);
    }
  } else {
    printf("Error Send To Client length=%d \n", nwritten);
  }
}

void appendWorkerData(int connfd, char *buffer, int len, int event_type,
                      char *from) {

  appnetPipeData data = {0};
  unsigned int datalen;
  data.len = len;
  data.proto = servG->connlist[connfd].proto_type;
  data.type = event_type;
  data.connfd = connfd;

  if (servG->connlist[connfd].proto_type == TCP) {
    int thid = servG->connlist[connfd].reactor_id;
    servG->reactor_threads[thid].hh->complete_length = 0;
  }

  datalen = PIPE_DATA_HEADER_LENG + data.len;
  aeEventLoop *reactor_el = getThreadEventLoop(connfd);
  int worker_id = servG->connlist[connfd].worker_id;

  setPipeWritable(reactor_el, worker_id, connfd);

  pthread_mutex_lock(&servG->workers[worker_id].w_mutex);

  /*append header */
  servG->workers[worker_id].send_buffer = sdscatlen(
      servG->workers[worker_id].send_buffer, &data, PIPE_DATA_HEADER_LENG);

  /*append data */
  if (len > 0) {
    servG->workers[worker_id].send_buffer =
        sdscatlen(servG->workers[worker_id].send_buffer, buffer, len);
  }

  pthread_mutex_unlock(&servG->workers[worker_id].w_mutex);
}

void appendHttpData(int connfd, char *header, int header_len, char *body,
                    int body_len, int event_type, char *from) {

  appnetPipeData data = {0};
  unsigned int datalen;
  data.header_len = header_len;
  data.len = header_len + body_len;
  data.type = event_type;
  data.connfd = connfd;
  data.proto = servG->connlist[connfd].proto_type;

  datalen = PIPE_DATA_HEADER_LENG + data.len;
  aeEventLoop *reactor_el = getThreadEventLoop(connfd);
  int worker_id = servG->connlist[connfd].worker_id;

  int ret = setPipeWritable(reactor_el, worker_id, connfd);
  if (ret == 0)
    printf("setPipeWritable error \n");

  pthread_mutex_lock(&servG->workers[worker_id].w_mutex);

  /* append pipe header data */
  servG->workers[worker_id].send_buffer = sdscatlen(
      servG->workers[worker_id].send_buffer, &data, PIPE_DATA_HEADER_LENG);

  if (data.len > 0) {

    /* append http header data */
    servG->workers[worker_id].send_buffer =
        sdscatlen(servG->workers[worker_id].send_buffer, header, header_len);

    /* append http body data */
    servG->workers[worker_id].send_buffer =
        sdscatlen(servG->workers[worker_id].send_buffer, body, body_len);
  }
  pthread_mutex_unlock(&servG->workers[worker_id].w_mutex);
}

int parseRequestMessage(int connfd, sds buffer, int len) {

  int ret;
  if (servG->proto_type == PROTOCOL_TYPE_TCP_ONLY) {

    servG->connlist[connfd].proto_type = TCP;
    appendWorkerData(connfd, buffer, len, PIPE_EVENT_MESSAGE,
                     "PROTOCOL_TYPE_TCP_ONLY");

    return BREAK_RECV;
  } else {

    if (isHttpProtocol(buffer, 8) == AE_TRUE ||
        servG->connlist[connfd].proto_type == WEBSOCKET ||
        servG->connlist[connfd].proto_type == HTTP) {

      if (servG->connlist[connfd].proto_type != WEBSOCKET) {

        servG->connlist[connfd].proto_type = HTTP;
        ret = httpRequestParse(connfd, buffer, sdslen(buffer));

      } else {

        int thid = servG->connlist[connfd].reactor_id;
        ret = wesocketRequestRarse(connfd, buffer, len,
                                   servG->reactor_threads[thid].hh,
                                   servG->reactor_threads[thid].hs);
      }
      return ret;
    } else {

      servG->connlist[connfd].proto_type = TCP;
      appendWorkerData(connfd, buffer, len, PIPE_EVENT_MESSAGE,
                       "PROTOCOL_TYPE_TCP");
      return BREAK_RECV;
    }
  }
}

void onClientReadable(aeEventLoop *el, int connfd, void *privdata, int mask) {

  appnetServer *serv = servG;
  ssize_t nread;
  unsigned int readlen, rcvbuflen, datalen;
  int worker_id = servG->connlist[connfd].worker_id;
  int thid = servG->connlist[connfd].reactor_id;
  char buffer[TMP_BUFFER_LENGTH];

  while (1) {

    nread = 0;
    memset(&buffer, 0, sizeof(buffer));
    nread = read(connfd, &buffer, sizeof(buffer));

    if (nread == -1 && errno == EAGAIN) {
      return; /* No more data ready. */
    }

    if (nread == 0) {
      onCloseByClient(el, privdata, serv, &serv->connlist[connfd]);
      return; /* Close event */
    } else if (nread > 0) {

      /* Append client recv_buffer */
      servG->connlist[connfd].recv_buffer =
          sdscatlen(servG->connlist[connfd].recv_buffer, &buffer, nread);

      int ret =
          parseRequestMessage(connfd, servG->connlist[connfd].recv_buffer,
                              sdslen(servG->connlist[connfd].recv_buffer));

      if (ret == BREAK_RECV) {
        int complete_length = servG->reactor_threads[thid].hh->complete_length;

        if (complete_length > 0) {
          sdsrange(servG->connlist[connfd].recv_buffer, complete_length, -1);
        } else {
          sdsclear(servG->connlist[connfd].recv_buffer);
        }
        break;

      } else if (ret == CONTINUE_RECV) {
        continue;

      } else if (ret == CLOSE_CONNECT) {
        freeClient(&servG->connlist[connfd]);
        return;
      }
      return;
    } else {
      printf("Recv Errno=%d,Err=%s \n", errno, strerror(errno));
    }
  }
}

int setPipeWritable(aeEventLoop *el, void *privdata, int connfd) {

  int worker_id = servG->connlist[connfd].worker_id;

  /* pipe index */
  int index =
      worker_id * servG->reactor_num + servG->connlist[connfd].reactor_id;

  if (sdslen(servG->workers[worker_id].send_buffer) == 0) {

    int ret = aeCreateFileEvent(el, servG->worker_pipes[index].pipefd[0],
                                AE_WRITABLE, onMasterPipeWritable, worker_id);

    if (ret == AE_ERR) {
      printf("setPipeWritable_error %s:%d \n", __FILE__, __LINE__);
    }
    return 1;
  }
  return 0;
}

aeEventLoop *getThreadEventLoop(int connfd) {
  int reactor_id = servG->connlist[connfd].reactor_id;
  return servG->reactor_threads[reactor_id].reactor.event_loop;
}

void acceptCommonHandler(appnetServer *serv, int connfd, char *client_ip,
                         int client_port) {

  if (serv->connect_num >= serv->connect_max) {
    printf("connect num over limit \n");
    close(connfd);
    return;
  }

  if ( connfd <= 0) {
    printf("error fd is null\n");
    close(connfd);
    return;
  }

  int reactor_id = connfd % serv->reactor_num;
  int worker_id = connfd % serv->worker_num;

  serv->connlist[connfd].client_ip = client_ip;
  serv->connlist[connfd].client_port = client_port;
  serv->connlist[connfd].connfd = connfd;
  serv->connlist[connfd].disable = 0;
  serv->connlist[connfd].send_buffer = sdsempty();
  serv->connlist[connfd].recv_buffer = sdsempty();
  serv->connlist[connfd].proto_type = 0;
  serv->connlist[connfd].worker_id = worker_id;
  serv->connlist[connfd].reactor_id = reactor_id;

  if (connfd != -1) {
    anetNonBlock(NULL, connfd);
    anetEnableTcpNoDelay(NULL, connfd);

    aeEventLoop *el = serv->reactor_threads[reactor_id].reactor.event_loop;
    if (aeCreateFileEvent(el, connfd, AE_READABLE, onClientReadable, &connfd) ==
        AE_ERR) {

      printf("CreateFileEvent read error fd =%d,errno=%d,errstr=%s  \n", connfd,
             errno, strerror(errno));

      close(connfd);
    }

    appnetPipeData data = {0};
    data.type = PIPE_EVENT_CONNECT;
    data.connfd = connfd;
    data.len = 0;
    serv->connect_num += 1;

    int index = worker_id * servG->reactor_num + reactor_id;

    int ret = aeCreateFileEvent(el, servG->worker_pipes[index].pipefd[0],
                                AE_WRITABLE, onMasterPipeWritable, worker_id);

    if (ret == AE_ERR)
      printf("Accept setPipeWritable Error \n");

    int sendlen = PIPE_DATA_HEADER_LENG;

    pthread_mutex_lock(&servG->workers[worker_id].w_mutex);

    /* append connect event message */
    servG->workers[worker_id].send_buffer =
        sdscatlen(servG->workers[worker_id].send_buffer, &data, sendlen);

    pthread_mutex_unlock(&servG->workers[worker_id].w_mutex);
  }
}

void onAcceptEvent(aeEventLoop *el, int fd, void *privdata, int mask) {

  if (servG->listenfd == fd) {
    int client_port, connfd, max = 1000;
    char client_ip[46];
    char neterr[1024];

    while (max--) {
      connfd =
          anetTcpAccept(neterr, fd, client_ip, sizeof(client_ip), &client_port);

      if (connfd == -1) {
        if (errno != EWOULDBLOCK) {
          printf("Accepting client Error connection: %s \n", neterr);
        }
        return;
      }

      acceptCommonHandler(servG, connfd, client_ip, client_port);
    }
  } else {
    printf("onAcceptEvent other fd=%d \n", fd);
  }
}

void runMainReactor(appnetServer *serv) {
  int res;
  res = aeCreateFileEvent(serv->main_reactor->event_loop, serv->listenfd,
                          AE_READABLE, onAcceptEvent, NULL);

  printf("Master Run pid=%d and listen socketfd=%d is ok? [%d]\n", getpid(),
         serv->listenfd, res == 0);
  printf("Server start ok ,You can exit program by Ctrl+C !!! \n");

  aeMain(serv->main_reactor->event_loop);
  aeDeleteEventLoop(serv->main_reactor->event_loop);
}

void masterKillHandler(int sig) {
  kill(0, SIGTERM);
  aeStop(servG->main_reactor->event_loop);
  stopReactorThread(servG);
  servG->running = 0;
}

void addSignal(int sig, void (*handler)(int), int restart) {
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = handler;
  if (restart == 1) {
    sa.sa_flags |= SA_RESTART;
  }
  sigfillset(&sa.sa_mask);
  assert(sigaction(sig, &sa, NULL) != -1);
}

void waitChild(int signo) {
  int status;
  int pid;
  while ((pid = waitpid(-1, &status, 0)) > 0) {
  }

  if (servG->exit_code == 1) {
    return;
  }
  servG->exit_code = 1;
}

void masterTermHandler(int signo) {}

void installMasterSignal(appnetServer *serv) {

  signal(SIGPIPE, SIG_IGN);
  addSignal(SIGINT, masterKillHandler, 1);
  signal(SIGTERM, masterTermHandler);
  signal(SIGCHLD, waitChild);
  //  signal (SIGKILL, masterSignalHandler );
  //  signal (SIGQUIT, masterSignalHandler );

  //  signal (SIGHUP, masterSignalHandler );
  //  signal(SIGSEGV, masterSignalHandler );
}

void createReactorThreads(appnetServer *serv) {
  int i, res;
  pthread_t threadid;
  void *thread_result;
  appnetReactorThread *thread;
  for (i = 0; i < serv->reactor_num; i++) {
    thread = &(serv->reactor_threads[i]);
    thread->param = zmalloc(sizeof(reactorThreadParam));
    thread->param->serv = serv;
    thread->param->thid = i;
    res = pthread_create(&threadid, NULL, reactorThreadRun,
                         (void *)thread->param);
    if (res != 0) {
      perror("Thread creat failed!");
      exit(0);
    }
    thread->thread_id = threadid;
  }
}

appnetReactorThread getReactorThread(appnetServer *serv, int i) {
  return (appnetReactorThread)(serv->reactor_threads[i]);
}

int randNumber(int min, int max) {
  int number = (rand() % (max - min + 1)) + min;
  return number;
}

int randTaskWorkerId() {
  int min, max, wid;
  min = servG->worker_num;
  max = servG->worker_num + servG->task_worker_num - 1;
  wid = randNumber(min, max);
  return wid;
}

void appendToClientSendBuffer(int connfd, char *buffer, int len) {
  if (sdslen(servG->connlist[connfd].send_buffer) == 0) {

    int reactor_id = connfd % servG->reactor_num;
    aeEventLoop *el = servG->reactor_threads[reactor_id].reactor.event_loop;
    int ret =
        aeCreateFileEvent(el, connfd, AE_WRITABLE, onClientWritable, NULL);

    if (ret == AE_ERR) {
      printf("appendToClientSendBuffer Error %s:%d \n", __FILE__, __LINE__);
      return;
    }
  }
  servG->connlist[connfd].send_buffer =
      sdscatlen(servG->connlist[connfd].send_buffer, buffer, len);
}

void readBodyFromPipe(aeEventLoop *el, int pipefd, appnetPipeData data) {
  int pos = PIPE_DATA_HEADER_LENG;
  int nread = 0;
  int bodylen = 0;
  if (data.len <= 0) {
    return;
  }
  char read_buff[TMP_BUFFER_LENGTH];

  while (1) {
    nread = read(pipefd, read_buff, data.len);
    if (nread > 0) {
      bodylen += nread;
      if (data.type == PIPE_EVENT_MESSAGE) {
        /* strcatlen function can extend space when current space not enough */
        if (sdslen(servG->connlist[data.connfd].send_buffer) == 0) {
          int ret = aeCreateFileEvent(el, data.connfd, AE_WRITABLE,
                                      onClientWritable, NULL);

          if (ret == AE_ERR) {
            printf("setPipeWritable_error %s:%d \n", __FILE__, __LINE__);
            return;
          }
        }

        servG->connlist[data.connfd].send_buffer = sdscatlen(
            servG->connlist[data.connfd].send_buffer, read_buff, nread);

      } else {
        asyncTask task;
        memcpy(&task, read_buff, sizeof(asyncTask));

        int worker_id, index;
        if (task.from < servG->worker_num) {
          worker_id = task.to + servG->worker_num;
          index = worker_id * servG->reactor_num + task.id % servG->reactor_num;
        } else {

          worker_id = task.to;
          index = worker_id * servG->reactor_num + task.id % servG->reactor_num;
        }

        if (sdslen(servG->workers[worker_id].send_buffer) == 0) {
          int result =
              aeCreateFileEvent(el, servG->worker_pipes[index].pipefd[0],
                                AE_WRITABLE, onMasterPipeWritable, worker_id);

          if (result == AE_ERR) {
            printf("setPipeWritable_error %s:%d \n", __FILE__, __LINE__);
          }
        }

        pthread_mutex_lock(&servG->workers[worker_id].w_mutex);
        /* append header */
        servG->workers[worker_id].send_buffer =
            sdscatlen(servG->workers[worker_id].send_buffer, &data,
                      PIPE_DATA_HEADER_LENG);

        /* append data */
        servG->workers[worker_id].send_buffer =
            sdscatlen(servG->workers[worker_id].send_buffer, read_buff, nread);
        pthread_mutex_unlock(&servG->workers[worker_id].w_mutex);
      }

      if (bodylen == data.len) {
        break;
      }
    }
  }

  if (bodylen <= 0) {
    if (nread == -1 && errno == EAGAIN) {
      return;
    }
    printf("readBodyFromPipe error\n");
    return;
  }
}

void onMasterPipeReadable(aeEventLoop *el, int fd, void *privdata, int mask) {
  int readlen = 0;
  appnetPipeData data;

  while ((readlen = read(fd, &data, PIPE_DATA_HEADER_LENG)) > 0) {
    if (readlen == 0) {
      close(fd);

    } else if (readlen == PIPE_DATA_HEADER_LENG) {
      if (data.type == PIPE_EVENT_MESSAGE) {

        if (servG->sendToClient) {
          readBodyFromPipe(el, fd, data);
        }

      } else if (data.type == PIPE_EVENT_TASK) {

        readBodyFromPipe(el, fd, data);
      } else if (data.type == PIPE_EVENT_CLOSE) {

        char proto_type = servG->connlist[data.connfd].proto_type;
        if (proto_type == WEBSOCKET) {
          int outlen = 0;
          char close_buff[1024];
          char *reason = "normal_close";

          wsMakeFrame(reason, strlen(reason), &close_buff, &outlen,
                      WS_CLOSING_FRAME);

          if (sdslen(servG->connlist[data.connfd].send_buffer) == 0) {

            int ret = aeCreateFileEvent(el, data.connfd, AE_WRITABLE,
                                        onClientWritable, NULL);

            if (ret == AE_ERR) {
              printf("setPipeWritable_error %s:%d \n", __FILE__, __LINE__);
            }
          }

          servG->connlist[data.connfd].send_buffer = sdscatlen(
              servG->connlist[data.connfd].send_buffer, close_buff, outlen);
        }

        if (servG->closeClient) {

          if (sdslen(servG->connlist[data.connfd].send_buffer) == 0) {
            servG->closeClient(&servG->connlist[data.connfd]);
          } else {
            servG->connlist[data.connfd].disable = 2;
          }
        }
      }
    } else {
      if (errno == EAGAIN) {
        return;
      }
    }
  }
}

void onMasterPipeWritable(aeEventLoop *el, int pipe_fd, void *privdata,
                          int mask) {
  ssize_t nwritten;
  int worker_id = (int)(privdata);
  pthread_mutex_lock(&servG->workers[worker_id].r_mutex);
  nwritten = write(pipe_fd, servG->workers[worker_id].send_buffer,
                   sdslen(servG->workers[worker_id].send_buffer));

  if (nwritten < 0) {
    printf("Master Pipe Write Error written=%d,errno=%d,errstr=%s \n", nwritten,
           errno, strerror(errno));
    close(pipe_fd);
    return;
  }

  if (nwritten > 0) {
    sdsrange(servG->workers[worker_id].send_buffer, nwritten, -1);
  } else {
    printf("Notice:onMasterPipeWritable empty loop \n");
  }

  /* if send_buffer empty, remove writable event */
  if (sdslen(servG->workers[worker_id].send_buffer) == 0) {
    aeDeleteFileEvent(el, pipe_fd, AE_WRITABLE);
  }

  pthread_mutex_unlock(&servG->workers[worker_id].r_mutex);
}

void *reactorThreadRun(void *arg) {
  reactorThreadParam *param = (reactorThreadParam *)arg;
  appnetServer *serv = param->serv;
  int thid = param->thid;
  aeEventLoop *el = aeCreateEventLoop(MAX_EVENT);
  serv->reactor_threads[thid].reactor.event_loop = el;
  serv->reactor_threads[thid].hh = (httpHeader *)malloc(sizeof(httpHeader));
  serv->reactor_threads[thid].hs = (handshake *)malloc(sizeof(handshake));

  int ret, i, index;
  for (i = 0; i < serv->worker_num + serv->task_worker_num; i++) {
    index = i * serv->reactor_num + thid;

    if (aeCreateFileEvent(el, serv->worker_pipes[index].pipefd[0], AE_READABLE,
                          onMasterPipeReadable, thid) == -1) {

      printf("CreateFileEvent error fd ");
      close(serv->worker_pipes[index].pipefd[0]);
    }
  }

  aeSetBeforeSleepProc(el, initThreadOnLoopStart);
  aeMain(el);
  aeDeleteEventLoop(el);

  free(serv->reactor_threads[thid].hh);
  free(serv->reactor_threads[thid].hs);
  el = NULL;
}

int socketSetBufferSize(int fd, int buffer_size) {
  if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size)) <
      0) {
    return AE_ERR;
  }
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size)) <
      0) {
    return AE_ERR;
  }
  return AE_OK;
}

void createWorkerProcess(appnetServer *serv) {
  char *neterr;
  int ret, i, j, index;
  int total_worker = serv->worker_num + serv->task_worker_num;

  for (i = 0; i < total_worker; i++) {
    serv->workers[i].send_buffer = sdsempty();
    // init mutex
    pthread_mutex_init(&(serv->workers[i].r_mutex), NULL);
    pthread_mutex_init(&(serv->workers[i].w_mutex), NULL);

    for (j = 0; j < serv->reactor_num; j++) {

      index = i * serv->reactor_num + j;
      ret =
          socketpair(PF_UNIX, SOCK_STREAM, 0, serv->worker_pipes[index].pipefd);
      assert(ret != -1);
      printf("woker_id=%d,pipe_index=%d,pipe0=%d,pipe1=%d \n", i, index,
             serv->worker_pipes[index].pipefd[0],
             serv->worker_pipes[index].pipefd[1]);
    }
  }

  for (i = 0; i < total_worker; i++) {

    serv->workers[i].pid = fork();
    if (serv->workers[i].pid < 0) {
      continue;
    } else if (serv->workers[i].pid > 0) {

      for (j = 0; j < serv->reactor_num; j++) {
        index = i * serv->reactor_num + j;
        close(serv->worker_pipes[index].pipefd[1]);

        anetSetSendBuffer(neterr, serv->worker_pipes[index].pipefd[0],
                          SOCKET_SND_BUF_SIZE);

        anetNonBlock(neterr, serv->worker_pipes[index].pipefd[0]);
      }
      continue;
    } else {

      for (j = 0; j < serv->reactor_num; j++) {
        index = i * serv->reactor_num + j;
        close(serv->worker_pipes[index].pipefd[0]);
        anetNonBlock(neterr, serv->worker_pipes[index].pipefd[1]);
        anetSetSendBuffer(neterr, serv->worker_pipes[index].pipefd[1],
                          SOCKET_SND_BUF_SIZE);
      }
      runWorkerProcess(i);
      exit(0);
    }
  }
}

void stopReactorThread(appnetServer *serv) {
  int i;
  for (i = 0; i < serv->reactor_num; i++) {
    aeStop(serv->reactor_threads[i].reactor.event_loop);
  }
}

void freeReactorThread(appnetServer *serv) {
  int i;
  for (i = 0; i < serv->reactor_num; i++) {
    pthread_cancel(serv->reactor_threads[i].thread_id);
    if (pthread_join(serv->reactor_threads[i].thread_id, NULL)) {
      printf("pthread join error \n");
    }
  }
  // free memory
  for (i = 0; i < serv->reactor_num; i++) {
    if (serv->reactor_threads[i].param) {
      zfree(serv->reactor_threads[i].param);
    }
  }
}

void freeWorkerBuffer(appnetServer *serv) {
  int i;
  for (i = 0; i < serv->worker_num; i++) {
    sdsfree(serv->workers[i].send_buffer);
  }
}
int freeConnectBuffers(appnetServer *serv) {
  int i;
  int count = 0;
  int minfd = 3; // TODO::
  if (serv->connect_num == 0) {
    return 0;
  }
  for (i = minfd; i < serv->connect_max; i++) {
    if (serv->connlist[i].disable == 0) {
      sdsfree(serv->connlist[i].send_buffer);
      sdsfree(serv->connlist[i].recv_buffer);
      // zfree( serv->connlist[i].hs );
      count++;
    }
    if (count == serv->connect_num) {
      break;
    }
  }
  return count;
}

void destroyServer(appnetServer *serv) {
  freeReactorThread(serv);

  freeConnectBuffers(serv);

  shm_free(serv->connlist, 1);
  if (serv->reactor_threads) {
    zfree(serv->reactor_threads);
  }

  freeWorkerBuffer(serv);
  if (serv->workers) {
    zfree(serv->workers);
  }
  if (serv->main_reactor) {
    zfree(serv->main_reactor);
  }
  if (serv != NULL) {
    zfree(serv);
  }
  puts("Master Exit ,Everything is ok !!!\n");
}

int setOption(char *key, char *val) {
  if (strcmp(key, OPT_WORKER_NUM) == 0) {
    if (atoi(val) <= 0) {
      return AE_FALSE;
    }
    servG->worker_num = atoi(val);
  } else if (strcmp(key, OPT_TASK_WORKER_NUM) == 0) {
    if (atoi(val) < 0) {
      return AE_FALSE;
    }
    servG->task_worker_num = atoi(val);
  } else if (strcmp(key, OPT_REACTOR_NUM) == 0) {
    if (atoi(val) <= 0) {
      return AE_FALSE;
    }
    servG->reactor_num = atoi(val);
  } else if (strcmp(key, OPT_MAX_CONNECTION) == 0) {
    if (atoi(val) <= 0) {
      return AE_FALSE;
    }
    servG->connect_max = atoi(val);
  } else if (strcmp(key, OPT_PROTOCOL_TYPE) == 0) {
    int type = atoi(val);
    if (type < 0 || type > PROTOCOL_TYPE_WEBSOCKET_MIX) {
      return AE_FALSE;
    }
    servG->proto_type = type;
  } else if (strcmp(key, APPNET_HTTP_DOCS_ROOT) == 0) {
    if (strlen(val) > sizeof(servG->http_docs_root)) {
      printf("Option Value Too Long!\n");
      return AE_FALSE;
    }

    memset(servG->http_docs_root, 0, sizeof(servG->http_docs_root));
    memcpy(servG->http_docs_root, val, strlen(val));
  } else if (strcmp(key, APPNET_HTTP_404_PAGE) == 0) {
    if (strlen(val) > sizeof(servG->http_404_page)) {
      printf("Option Value Too Long!\n");
      return AE_FALSE;
    }

    memset(servG->http_404_page, 0, sizeof(servG->http_404_page));
    memcpy(servG->http_404_page, val, strlen(val));
  } else if (strcmp(key, APPNET_HTTP_50X_PAGE) == 0) {
    if (strlen(val) > sizeof(servG->http_50x_page)) {
      printf("Option Value Too Long!\n");
      return AE_FALSE;
    }
    memset(servG->http_50x_page, 0, sizeof(servG->http_50x_page));
    memcpy(servG->http_50x_page, val, strlen(val));
  } else if (strcmp(key, APPNET_HTTP_UPLOAD_DIR) == 0) {
    if (strlen(val) > sizeof(servG->http_upload_dir)) {
      printf("Option Value Too Long!\n");
      return AE_FALSE;
    }
    memset(servG->http_upload_dir, 0, sizeof(servG->http_upload_dir));
    memcpy(servG->http_upload_dir, val, strlen(val));
  } else if (strcmp(key, APPNET_HTTP_KEEP_ALIVE) == 0) {
    int keep = atoi(val);
    if (keep < 0) {
      return AE_FALSE;
    }
    servG->http_keep_alive = keep;
  } else if (strcmp(key, OPT_DAEMON) == 0) {
    int daemon = atoi(val);
    if (daemon < 0 || daemon > 2) {
      return AE_FALSE;
    }
    servG->daemon = daemon;
  } else {
    printf("Unkown Option %s \n", key);
    return AE_FALSE;
  }

  if (servG->reactor_num > servG->worker_num) {
    servG->reactor_num = servG->worker_num;
  }
  return AE_TRUE;
}

void initServer(appnetServer *serv) {

  serv->connlist = shm_calloc(serv->connect_max, sizeof(appnetConnection));

  serv->reactor_threads =
      zmalloc(serv->reactor_num * sizeof(appnetReactorThread));

  serv->workers = zmalloc((serv->worker_num + serv->task_worker_num) *
                          sizeof(appnetWorkerProcess));

  serv->worker_pipes = zmalloc((serv->worker_num + serv->task_worker_num) *
                               serv->reactor_num * sizeof(appnetWorkerPipes));

  srand((unsigned)time(NULL));
  serv->main_reactor = zmalloc(sizeof(appnetReactor));
  serv->main_reactor->event_loop = aeCreateEventLoop(10);

  aeSetBeforeSleepProc(serv->main_reactor->event_loop, initOnLoopStart);
  installMasterSignal(serv);
}

void set_daemon(appnetServer *serv) {
  if (serv->daemon == 0)
    return;

  int ret;
  if (serv->daemon == 1) {
    ret = daemon(1, 0);
  } else if (serv->daemon == 2) {
    ret = daemon(1, 1);
  }

  if (ret < 0) {
    perror("error daemon...\n");
    exit(1);
  }
}

int startServer(appnetServer *serv) {
  int sockfd[2];
  int sock_count = 0;

  set_daemon(serv);
  // memory alloc
  initServer(serv);

  listenToPort(serv->listen_ip, serv->port, sockfd, &sock_count);

  serv->listenfd = sockfd[0];

  createWorkerProcess(serv);

  createReactorThreads(serv);

  __SLEEP_WAIT__;

  runMainReactor(serv);

  destroyServer(serv);

  __SLEEP_WAIT__;
  return 0;
}

appnetServer *appnetServerCreate(char *ip, int port) {
  appnetServer *serv = (appnetServer *)zmalloc(sizeof(appnetServer));
  serv->listen_ip = ip;
  serv->port = port;
  serv->connect_num = 0;
  serv->reactor_num = 1;
  serv->worker_num = 1;
  serv->task_worker_num = 1;
  serv->connect_max = 1024;
  serv->exit_code = 0;
  serv->daemon = 0;
  serv->http_keep_alive = 1;
  serv->proto_type = PROTOCOL_TYPE_WEBSOCKET_MIX;
  serv->runForever = startServer;
  serv->send = sendMessageToReactor;
  serv->close = sendCloseEventToReactor;
  serv->setOption = setOption;
  serv->setHeader = setHeader;
  serv->sendToClient = anetWrite;
  serv->closeClient = freeClient;

  memset(serv->http_docs_root, 0, sizeof(serv->http_docs_root));
  memset(serv->http_404_page, 0, sizeof(serv->http_404_page));
  memset(serv->http_50x_page, 0, sizeof(serv->http_50x_page));
  memset(serv->http_upload_dir, 0, sizeof(serv->http_upload_dir));

  memcpy(serv->http_docs_root, DEFAULT_HTTP_DOCS_ROOT,
         strlen(DEFAULT_HTTP_DOCS_ROOT));
  memcpy(serv->http_404_page, DEFALUT_HTTP_404_PAGE,
         strlen(DEFALUT_HTTP_404_PAGE));
  memcpy(serv->http_50x_page, DEFALUT_HTTP_50X_PAGE,
         strlen(DEFALUT_HTTP_50X_PAGE));

  servG = serv;
  return serv;
}
