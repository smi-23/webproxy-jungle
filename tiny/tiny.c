/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"
// #include <strings.h>
// #include <stdlib.h>
// #define original_staticx

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* MAXLINE, MAXBUF == 8192 */
/* 설명 1.1 */
// 입력 ./tiny 8000 / argc = 2, argv[0] = tiny, argv[1] = 8000
int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  // client socket's length
  socklen_t clientlen;
  // client socket's addr
  struct sockaddr_storage clientaddr;

  /* Check command-line args */
  // port number 입력 X => error
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* Open_listnedfd 함수를 호출해서 듣기 소켓을 오픈한다. 인자로 포트번호를 넘겨준다. */
  // Open_listenfd는 요청받을 준비가 된 듣기 식별자를 리턴한다 = listenfd
  // 설명 1.2
  listenfd = Open_listenfd(argv[1]);

  /* 전형적인 무한 서버 루프를 실행 */
  while (1)
  {
    // accept 함수 인자에 넣기 위한 주소 길이를 계산
    clientlen = sizeof(clientaddr);

    /* 반복적으로 연결 요청을 접수 */
    // accept 함수는 1. 듣기 식별자, 2. 소켓주소구조체의 주소, 3. 주소(소켓구조체)의 길이를 인자로 받는다.
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept

    // Getaddrinfo는 호스트 이름 : 호스트 주소, 서비스 이름: 포트 번호의 스트링 표시를 소켓 주소 구조체로 변환
    // Getnameinfo는 위를 반대로 소켓 주소 구조체에서 스트링 표시로 변환
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    // 트랜젝션을 수행
    doit(connfd); // line:netp:tiny:doit

    // 트랜잭션이 수행된 후 자신 쪽의 연결 끝(소켓)을 닫는다.
    Close(connfd); // line:netp:tiny:close
  }
}

// 응답을 해주는 함수
void doit(int fd)
{
  // 정적 파일인지 아닌지를 판단해주기 위한 변수
  int is_static;
  // 파일에 대한 정보를 가지고 있는 구조체
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];

  // rio 설명 2.1
  // rio_readlineb를 위해  rio_t 타입(구조체)의 읽기 버퍼를 선언
  rio_t rio;

  /* Read request line and headers */
  // Rio = robust I/O
  // rio_t 구조체를 초기화 해준다.
  Rio_readinitb(&rio, fd);

  // buf에서 client request 읽어들이기
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  // request header 출력
  printf("%s", buf);
  // buf에 있는 데이터를 method, url, version에 담기
  // 설명 2.2
  sscanf(buf, "%s %s %s", method, uri, version);

  /* Tiny 는 GET method 만 지원하기에 클라이언트가 다른 메소드를 요청하면 에러메세지를 보내고, main routin으로 돌아온다. */
  // strcmp() 문자열 비교 함수
  // strcasecmp() 대소문자를 무시하는 문자열 비교 함수
  // strncasecmp() 대문자를 무시하고, 지정한 길이만큼 문자열을 비교하는 함수
  // problem 11.11 을 위해 HEAD 추가
  if (strcasecmp(method, "GET"))
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  // GET method라면 읽어들이고, 다른 요청 헤더들은 무시한다.
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  // URI 를 파일 이름과 비어 있을 수도 있는 CGI 인자 스트링으로 분석하고, 요청이 정적 또는 동적 컨텐츠를 위한 것인지 나타내느 플래그를 설정한다.
  is_static = parse_uri(uri, filename, cgiargs);

  // 만일 파일이 디스크상에 있지 않으면, 에러메시지를 즉시 클라이언트에게 보내고 메인 루틴으로 리턴
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  /* Serve static content */
  if (is_static)
  {
    // 정적 컨텐츠이고, 이 파일이 보통 파일인지, 읽기 권한을 가지고 있는지 검증한다.
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read this file");
      return;
    }
    // 그렇다면 정적 컨텐츠를 클라이언트에게 제공
    serve_static(fd, filename, sbuf.st_size);
  }
  /* Serve dynamic content */
  else
  {
    // 실행 가능한 파일인지 검증
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 그렇다면 동적 컨텐츠를 클라이언트에게 제공
    serve_dynamic(fd, filename, cgiargs);
  }
}

// 명백한 오류에 대해서 클라이언트에게 보고하는 함수
// HTTP응답을 응답 라인에 적절한 상태 코드와 상태 메시지를 함께 클라이언트에게 보낸다.
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  /* 브라우저 사용자에게 에러를 설명하는 응답 본체에 HTML도 함께 보낸다 */
  /* HTML 응답은 본체에서 컨텐츠의 크기와 타입을 나타내야하기에, HTMl 컨텐츠를 한 개의 스트링으로 만든다. */
  /* 이는 sprintf를 통해 body는 인자에 스택되어 하나의 긴 스트리잉 저장된다. */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  // body 배열에 차곡차곡 html 을 쌓아 넣어주고

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);

  /* Rio_writen 그림 10.3 */
  // 굳이 보내고 쌓고 보내고 쌓고가 아니라 위에 body처럼 쭉 쌓아서 한번에 보내도 되지않을까?
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  // buf에 넣고 보내고 넣고 보내고

  // sprintf로 쌓아놓은 길쭉한 배열을 (body, buf)를 보내준다.
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* Tiny는 요청 헤더 내의 어떤 정보도 사용하지 않는다, 단순히 이들을 읽고 무시한다. */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);

  // header 마지막 줄은 비어있기에 \r\n 만 buf에 담겨있다면 while문을 탈출한다.
  while (strcmp(buf, "\r\n"))
  {
    // rio 설명을 보면 rio_readlineb는 \n을 만날때 멈춘다.
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    // 멈춘 지점 까지 출력하고 다시 while
  }
  return;
}

/* Parse URI from GET request */
// uri를 파일 이름과 비어 있을 수도 있는 CGI 인자 스트링으로 분석하고, 요청이 정적 또는 동적 컨텐츠를 위한 것인지 나타내는 플래그를 설정한다.
// uri 예시 static : /mp4sample.mp4 , / , /adder.html
// dynamic : /cgi-bin/addr?first=1213&second=1232
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  // strstr으로 cgi-bin이 들어있는지 확인하고 양수값을 리턴하면 dynamic content를 요구하는 것이기에 조건문을 탈출
  // static content
  if (!strstr(uri, "cgi-bin"))
  {
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);

    // uri 문자열 끝이 / 일 경우 허전하지 말라고 home.html을 filename에 붙혀준다.
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");
    return 1;
  }

  // dynamic content
  else
  {
    ptr = index(uri, '?'); // index 함수는 문자열에서 특정 문자의 위치를 반환한다.

    // ? 가 존재한다면
    if (ptr)
    {
      // 인자로 주어진 값들을 cgiargs 변수에 넣는다.
      strcpy(cgiargs, ptr + 1);

      // ptr 초기화
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF]; //, *fbuf

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer : Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection : close\r\n", buf);
  sprintf(buf, "%sContent-length : %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type : %s\r\n\r\n", buf, filetype);

  /* writen = client 쪽에 */
  Rio_writen(fd, buf, strlen(buf));

  /* 서버 쪽에 출력 */
  printf("Response headers:\n");
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);

  /* 숙련 문제 11.9 */
  // fbuf = malloc(filesize);
  // Rio_readn(srcfd, fbuf, filesize);
  // Close(srcfd);
  // Rio_writen(fd, fbuf, filesize);
  // free(fbuf);
}

/* get_filetype - Derive file type from filename */
// strstr 두번째 인자가 첫번째 인자에 들어있는지 확인
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");

  // //  11.7 숙련 문제 = Tiny 가 MPG 비디오 파일을 처리하도록 하기 (정글에선 MP4)
  // else if (strstr(filename, ".mp4"))
  //   strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0)
  { /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);
    // method를 cgi-bin/adder.c에 넘겨주기 위해 환경변수 set
    // setenv("REQUEST_METHOD", method, 1);
    Dup2(fd, STDOUT_FILENO);              /* Redirect stdout to client */
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  Wait(NULL); /* Parent waits for and reaps child */
}
