#include <stdio.h>
#include "csapp.h"

#include "cache.h"
#include "cache.c"

/* Recommended max cache and object sizes */
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static cache *proxy_cache;
void read_header(int fd, char *host, char *port, char *url);
void read_requesthdrs(rio_t *rp);
void doit(int fd);
void send_request(char *host, char *port, char *uri, int connfd);
void *thread(void *varagp);

/*
웹프록시 서버의 주 진입점, 지정된 포트에서 클라이언트의 연결을 수락하고, 각 연결을 처리하기 위해 쓰레드 생성
*/
int main(int argc, char **argv)
{
  int listenfd, *connfd;                 // 서버 소켓 및 클라이언트 소켓 식별자 변수
  char hostname[MAXLINE], port[MAXLINE]; // 호스트 이름과 포트 번호를 저장할 변수
  proxy_cache = new_cache();             // 캐시 객체 초기화
  char buf[MAXLINE];                     // 일시적으로 문자열을 저장할 버퍼
  socklen_t clientlen;                   // 클라이언트 주소 구조체 크기 변수
  struct sockaddr_storage clientaddr;    // 클라이언트 주소 정보를 저장할 구조체
  pthread_t tid;                         // 스레드 식별자 변수

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // 명령행 인수 개수 검사
    exit(1);                                        // 잘못된 인수가 전달되었을 때 프로그램 종료
  }

  listenfd = Open_listenfd(argv[1]); // 지정된 포트로 서버 소켓 열기
  while (1)
  {
    clientlen = sizeof(clientaddr);
    /* pthread_create의 경우 argp 인자가 void* 이다.
    따라서 연결 식별자를 인자로 넣어줄 수 있게 안전하게 포인터를 만들어준다. */
    connfd = Malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 포인터가 가리키는 값을 연결 식별자 값으로.
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    /*
        - 첫 번째 인자: 생성된 스레드의 식별자를 저장할 변수
        - 두 번째 인자: 스레드 특성 지정 (기본값 NULL 사용)
        - 세 번째 인자: 스레드가 실행할 함수 (thread 함수)
        - 네 번째 인자: 스레드 함수에 전달할 인자 (connfd 포인터, 클라이언트 소켓 식별자)
    */
    Pthread_create(&tid, NULL, thread, connfd);
  }
}

/*
각 클라이언트 연결을 처리하는 쓰레드 함수, 쓰레드는 소켓 식별자를 받아와 해당 클라이언트와 통신
쓰레드는 자신을 분리하고, 동작이 끝난 후 소켓을 닫음
*/
void *thread(void *vargp)
{                               // 스레드를 이용해서 동시성 구현
  int connfd = *((int *)vargp); // 인자로 소켓 식별자 받기
  printf("connect\n");
  Pthread_detach(pthread_self()); // 스레드 분리 (스레드가 종료되었을 때  자원 자동 해제)
  // 각각의 연결이 별도의 쓰레드에 의해서 독립적으로 처리 -> 서버가 명시적으로 각각의 피어 쓰레드 종료하는 것 불필요 -> detach
  // 메모리 누수를 방지하기 위해서 사용
  Free(vargp);   // 동적 할당한 파일 식별자 포인터를 free해준다.
  doit(connfd);  // 주요 로직 실행하기
  Close(connfd); // 로직이 끝나면 소켓 닫기
  return NULL;
}

/* 주요 로직 */
/*
클라이언트의 요청을 처리하는 함수, HTTP 헤더를 파싱하고, URI를 추출한다.
추출한 URI를 사용하여 서버로부터 데이터를 가져오는 send_request 함수를 호출한다.
*/
void doit(int connfd)
{
  /* 1. 클라이언트로부터 받은 요청의 헤더를 읽고 필요한 정보를 저장 */
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 헤더 정보를 저장할 변수들
  rio_t rio;

  Rio_readinitb(&rio, connfd);            // Rio 라이브러리를 사용하여 입력 스트림 초기화
  if (!Rio_readlineb(&rio, buf, MAXLINE)) // 첫 번째 헤더 라인을 읽지 못하면 함수 종료
    return;
  printf("%s", buf);                             // 읽은 헤더 라인을 출력
  sscanf(buf, "%s %s %s", method, uri, version); // 헤더 라인을 파싱하여 메서드, URI, 버전을 추출
  read_requesthdrs(&rio);                        // 나머지 헤더를 처리하기 위해 함수 호출

  /* 2. 받은 URI를 파싱하여 호스트와 포트를 추출. */
  char *end_host[MAXLINE], *end_port[MAXLINE], *end_uri[MAXLINE]; // 파싱 결과를 저장할 변수들
  parse_uri(uri, end_host, end_port);                             // URI를 파싱하여 호스트와 포트를 추출

  /* 3. 파싱한 정보를 기반으로 URI를 재구성하고, 서버에 요청을 보내고 응답을 클라이언트에 전달 */
  send_request(end_host, end_port, uri, connfd); // 파싱한 정보를 사용하여 서버에 요청 보내기
}

/* 서버에 요청하는 함수*/
/*
서버에 HTTP GET요청을 보내고, 서버로부터 응답을 받아 클라이언트에게 전달하는 함수,
캐시에서 이전에 저장된 데이터를 검색하고, 캐시에서 데이터를 찾으면 해당 데이터를 클라이언트에게 바로전송,
그렇지 않으면 서버에 요청을 보내고 응답을 클라이언트에게 전송하면서 동시에 데이터를 캐시에 저장한다.
*/
void send_request(char *host, char *port, char *uri, int fd)
{

  int clientfd;                    // 소켓 식별자
  char buf[MAXLINE];               // 값 저장할 애들
  char data[MAX_OBJECT_SIZE] = ""; // 캐시 데이터를 임시 저장할 장소

  // 캐시값을 찾는것! -> 해당 URI의 데이터가 존재하는 경우
  if (find_cache(proxy_cache, uri, data))
  {
    Rio_writen(fd, data, MAXLINE); // 캐시 값 그대로 클라이언트에게 바로 보낸다
  }
  // 캐시에 값을 못 찾으면?
  else
  {
    clientfd = Open_clientfd(host, port);          // 서버랑 연결할 클라이언트 소켓 생성
    sprintf(buf, "GET /%s HTTP/1.1\r\n\r\n", uri); // HTTP GET 요청 헤더 생성
    Rio_writen(clientfd, buf, strlen(buf));        // 서버에 HTTP 요청 헤더 보내기

    rio_t rio;
    size_t n;
    Rio_readinitb(&rio, clientfd); // 파일 보내기  ,서버 응답을 읽기위한 rio 초기화

    // 서버 응답 확인하고 클라이언트에 보내주기
    int cache_size_check = 1; // 캐시의 크기가 넘는지 확인하기
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
      Rio_writen(fd, buf, n); // 응답 바로 클라이언트에 보내주기

      // 응답 데이터의 크기가 최대 캐시 객체 크기를 넘지 않으면 데이터를 임시 저장
      if (strlen(data) + strlen(buf) < MAX_OBJECT_SIZE)
        strcat(data, buf); // 저장할 데이터를 data에 계속 더하기
      else
        cache_size_check = 0; // 캐시 크기가 최대 크기를 넘는 경우
    }
    Close(clientfd);

    // 응답 사이즈가 최대 사이즈보다 작으면 캐시에 값 넣기
    if (cache_size_check == 1)
      insert_cache(proxy_cache, uri, data);
  }
}

/* 헤더 처리하는 함수 */
/*
클라이언트로부터 받은 HTTP 요청 헤더를 읽고 출력하는 함수
*/
void read_requesthdrs(rio_t *rio)
{
  char buf[MAXLINE]; // 읽은 헤더를 임시로 저장할 변수

  Rio_readlineb(rio, buf, MAXLINE); // 첫 번째 헤더 라인을 읽어옴
  printf("%s", buf);                // 읽은 헤더를 화면에 출력

  // 빈 줄('\r\n')을 만날 때까지 헤더를 계속 읽고 출력
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rio, buf, MAXLINE); // 다음 헤더 라인을 읽어옴
    printf("%s", buf);                // 읽은 헤더를 화면에 출력
  }
  return;
}

/*
uri 파싱해서 호스트, 포트 및 경로 정보를 추적
 */
int parse_uri(char *uri, char *host, char *port)
{
  char *token; // 토큰으로 분리할 때 사용할 포인터

  // 호스트 부분 찾기
  token = strtok(uri, "/");  // URI에서 첫 번째 슬래시('/') 전까지를 호스트로 판단합니다.
  token = strtok(NULL, ":"); // ':' 문자를 기준으로 다음 토큰을 찾습니다.
  strcpy(host, ++token);     // 호스트 문자열을 복사합니다.

  // 포트 부분 찾기
  token = strtok(NULL, "/"); // 다음 슬래시('/') 전까지를 포트로 판단합니다.
  if (token != NULL)
  {
    strcpy(port, token); // 포트 문자열을 복사합니다.
  }
  else
  {
    strcpy(port, "80"); // 포트가 없는 경우 기본값으로 80을 설정합니다.
  }

  // URI 경로 찾기
  token = strtok(NULL, ""); // 남은 부분은 URI 경로로 판단합니다.
  if (token != NULL)
  {
    strcpy(uri, token); // URI 경로 문자열을 복사합니다.
  }
  else
  {
    strcpy(uri, "/"); // URI 경로가 없는 경우 기본값으로 '/'을 설정합니다.
  }
}
/* $end proxy */