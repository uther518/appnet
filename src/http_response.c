
#include "http_response.h"
#include "ae.h"
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

void onRespWritable(aeEventLoop *el, int connfd, void *privdata, int mask) {
  httpHeader *req_header = (httpHeader *)privdata;
  httpResponseStaticProc(req_header);
  aeDeleteFileEvent(el, connfd, AE_WRITABLE);
}

void httpResponseStatic(httpHeader *req_header) {
  int thid = req_header->connfd % servG->reactor_num;
  aeEventLoop *el = servG->reactor_threads[thid].reactor.event_loop;

  httpHeader *header = malloc(sizeof(httpHeader));
  memcpy(header, req_header, sizeof(httpHeader));

  if (aeCreateFileEvent(el, req_header->connfd, AE_WRITABLE, onRespWritable,
                        header) == -1) {

    printf("setPipeWritable_error %s:%d \n", __FILE__, __LINE__);
    return;
  }
}

void httpResponseWrite(int connfd, char *buff, int len) {
  int nwritten = write(connfd, buff, len);
  if (nwritten <= 0) {
    printf("I/O error writing to client,connfd=%d,len=%d: %s \n", connfd, len,
           strerror(errno));
    return;
  }
}

headerStatus getHttpStatus(int status) {
  headerStatus header_status;
  if (status < http_status_200 || status > http_status_max) {
    return header_status;
  }

  int code_i = status % 100;
  if (status < http_status_300) {
    return http_status_20x[code_i];
  }

  if (status < http_status_400) {
    return http_status_30x[code_i];
  }

  if (status < http_status_500) {
    return http_status_40x[code_i];
  }

  if (status < http_status_max) {
    return http_status_50x[code_i];
  }
  return header_status;
}

int isDir(char *path) {
  int s;
  struct stat info;
  s = stat(path, &info);
  return (s == 0 && (info.st_mode & S_IFDIR));
}

int isFile(char *path) {
  int s;
  struct stat info;
  s = stat(path, &info);
  return (s == 0 && (info.st_mode & S_IFREG));
}

int appendHeaderBuffer(headerOut *header_out, char *data, int len) {
  memcpy(header_out->data + header_out->length, data, len);
  header_out->length += len;
  header_out->count += 1;
}

int appendRespHeader(headerOut *header_out, int line_type, ...) {
  char line[1024] = {0};
  int len = 0;
  char *arg_string;
  int arg_int;
  va_list ap;
  va_start(ap, line_type);

  switch (line_type) {
  case HEADER_STATUS:
    // error_code,
    arg_string = va_arg(ap, char *);
    len = snprintf(line, sizeof(line), header_formats[HEADER_STATUS],
                   header_out->req->version, arg_string);
    break;
  case HEADER_SERVER:
    len = snprintf(line, sizeof(line), http_server_full_string);
    break;
  case HEADER_CONTENT_TYPE:
    len = snprintf(line, sizeof(line), header_formats[HEADER_CONTENT_TYPE],
                   header_out->req->mime_type);
    break;
  case HEADER_CONTENT_LENGTH:
    // content-length
    arg_int = va_arg(ap, int);
    len = snprintf(line, sizeof(line), header_formats[HEADER_CONTENT_LENGTH],
                   arg_int);
    break;
  case HEADER_LOCATION:
    // content-length
    arg_string = va_arg(ap, char *);
    len = snprintf(line, sizeof(line), header_formats[HEADER_LOCATION],
                   arg_string);
    break;
  case HEADER_KEEP_ALIVE:
    if (header_out->req->keep_alive == 0) {
      len = snprintf(line, sizeof(line), header_formats[HEADER_KEEP_ALIVE],
                     "close");
    } else {
      len = snprintf(line, sizeof(line), header_formats[HEADER_KEEP_ALIVE],
                     "keep-alive");
    }
    break;
  case HEADER_END_LINE:
    len = snprintf(line, sizeof(line), header_formats[HEADER_END_LINE]);
    break;
  }
  appendHeaderBuffer(header_out, line, strlen(line));
  return len;
}

// this function only process status=200 page, not include resp_error_page
void createCommonHeader(headerOut *header_out, int status_code) {
  headerStatus error_page = getHttpStatus(status_code);
  // header append
  appendRespHeader(header_out, HEADER_STATUS, error_page.status);
  appendRespHeader(header_out, HEADER_SERVER);
  appendRespHeader(header_out, HEADER_CONTENT_TYPE);
  appendRespHeader(header_out, HEADER_KEEP_ALIVE);
}

void httpRedirect(httpHeader *req_header, char *uri) {
  headerOut header_out;
  memset(&header_out, 0, sizeof(header_out));
  header_out.req = req_header;
  headerStatus error_page = getHttpStatus(301); // header append

  createCommonHeader(&header_out, 301);
  appendRespHeader(&header_out, HEADER_LOCATION, uri);
  appendRespHeader(&header_out, HEADER_CONTENT_LENGTH, 0);
  appendRespHeader(&header_out, HEADER_END_LINE);

  httpResponseWrite(req_header->connfd, header_out.data, header_out.length);
  httpClose(req_header, 0);
}

int pageIsDefined(char *page) {
  char path[128] = {0};
  getFilePath(page, path);
  if (isFile(path)) {
    return 1;
  }
  printf("%s Is Not Defined Page \n", path);
  return 0;
}

//
int respDefinedErrorPage(headerOut *header_out, int err_code) {
  switch (err_code) {
  case 404:
    if (pageIsDefined(servG->http_404_page) == 0)
      return 0;
    httpRedirect(header_out->req, servG->http_404_page);
    break;
  case 500:
  case 501:
  case 502:
  case 503:
  case 504:
  case 505:
    if (pageIsDefined(servG->http_50x_page) == 0)
      return 0;
    httpRedirect(header_out->req, servG->http_50x_page);
    break;
  default:
    return 0;
  }
  return 1;
}

void headerAppendLength(headerOut *header_out, int len) {
  appendRespHeader(header_out, HEADER_CONTENT_LENGTH, len);
}

//如果内容是动态生成的数据流,则需要分段返回给客户端
void appendChunkedHeader(headerOut header_out) {
  // appendRespHeader( header_out , HEADER_CONTENT_TYPE  );
  // appendRespHeader( header_out , HEADER_CONTENT_LENGTH  , datalen );
}

// 404
void respErrorPage(headerOut *header_out, int status_code) {
  if (status_code >= 400 && status_code <= 507) {
    int ret = respDefinedErrorPage(header_out, status_code);
    if (ret == AE_TRUE) {
      return;
    }
  }

  // get header info
  headerStatus error_page = getHttpStatus(status_code);
  int datalen = strlen(error_page.data);

  // header append
  createCommonHeader(header_out, status_code);
  headerAppendLength(header_out, datalen);
  appendRespHeader(header_out, HEADER_END_LINE);

  // send
  httpResponseWrite(header_out->req->connfd, header_out->data,
                    header_out->length);
  if (header_out->req->nobody != AE_TRUE) {
    httpResponseWrite(header_out->req->connfd, error_page.data, datalen);
  }
  // nobody强制关闭
  httpClose(header_out->req, header_out->req->nobody);
}

void httpClose(httpHeader *req_header, int force) {

  if (req_header->keep_alive == AE_FALSE ||
      servG->http_keep_alive == AE_FALSE || force == AE_TRUE) {
    appnetConnection conn = servG->connlist[req_header->connfd];
    freeClient(&conn);
  }
  free(req_header);
}

void httpResponseStaticProc(httpHeader *req_header) {

  int len, cllen, ctlen;
  char path[1024] = {0};
  headerOut header_out;
  memset(&header_out, 0, sizeof(header_out));
  header_out.req = req_header;

  getFilePath(req_header->uri, path);
  struct stat stat_file;
  int ret = stat(path, &stat_file);

  if (ret < 0) {
    respErrorPage(&header_out, 404);
    return;
  }

  createCommonHeader(&header_out, 200);
  headerAppendLength(&header_out, stat_file.st_size);
  appendRespHeader(&header_out, HEADER_END_LINE);

  int nwritten = write(req_header->connfd, header_out.data, header_out.length);
  if (nwritten <= 0) {
    printf("I/O error writing to client connfd=%d,len=%d: %s \n",
           req_header->connfd, header_out.length, strerror(errno));
    return;
  }

  if (req_header->nobody == AE_TRUE) {
    httpClose(req_header, 1);
    return;
  }

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    printf("Open file Error:%s,errno=%d \n", strerror(errno), errno);
    return;
  }

  // setsockopt (fd, SOL_TCP, TCP_CORK, &on, sizeof (on));
  off_t offset = 0;
  int force_close = 0;
  while (offset < stat_file.st_size) {
    int sendn =
        sendfile(req_header->connfd, fd, &offset, stat_file.st_size - offset);

    if (sendn < 0) {
      //如果socket缓冲区不可用，则挂起等待可用
      if (errno == EAGAIN || errno == EINTR) {
        if (anetHandup(req_header->connfd, 5000, AE_WRITABLE) < 0) {
          //如果超时，退出
          printf("Sendfile anetHandup timeout.......\n");
          force_close = 1;
          break;
        } else {
          //否则继续发送
          continue;
        }
      } else {
        break;
      }
    }
  }
  close(fd);
  httpClose(req_header, force_close);
}

void getFilePath(char *uri, char *path) {

  char *pos = strstr(uri, "?");
  memcpy(path, servG->http_docs_root, strlen(servG->http_docs_root));

  if (pos == NULL) {
    memcpy(path + strlen(servG->http_docs_root), uri, strlen(uri));
  } else {
    memcpy(path + strlen(servG->http_docs_root), uri, pos - uri);
  }
}
