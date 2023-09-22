#include <stdio.h>
#include "csapp.h"

#include "cache.h"
#include "cache.c"

/* 과제 조건: HTTP/1.0 GET 요청을 처리하는 기본 시퀀셜 프록시

  클라이언트가 프록시에게 다음 HTTP 요청 보내면
  GET http://www.google.com:80/index.html HTTP/1.1
  프록시는 이 요청을 파싱해야한다 ! 호스트네임, www.google.com, /index.html

  이렇게 프록시는 서버에게 다음 HTTP 요청으로 보내야함.
  GET /index.html HTTP/1.0

  즉, proxy는 HTTP/1.1로 받더라도 server에는 HTTP/1.0으로 요청해야함
*/

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char *new_version = "HTTP/1.0";

/* Function prototyps => 함수 순서 상관이 없게됨 */
void do_it(int fd);
void do_request(int clientfd, char *method, char *uri_ptos, char *host);
void do_response(int connfd, int clientfd);
int parse_uri(char *uri, char *uri_ptos, char *host, char *port);
void *thread(void *vargp);

int main(int argc, char **argv)
{
    int listenfd, *connfdp;                // 서버 소켓 및 클라이언트 소켓 식별자 변수
    char hostname[MAXLINE], port[MAXLINE]; // 호스트 이름과 포트 번호를 저장할 변수
    socklen_t clientlen;                   // 클라이언트 주소 구조체 크기 변수
    struct sockaddr_storage clientaddr;    // 클라이언트 주소 정보를 저장할 구조체
    pthread_t tid;                         // 스레드 식별자 변수

    /* Check command line args */
    if (argc != 2)
    { // 명령행 인수 개수 검사
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1); // exit(0) => exit(1) 수정, 잘못된 인수가 전달되었을 때 프로그램 종료
    }
    // Review from PGJ
    // exit 함수의 경우 통상적으로 정상적인 종료를 했을 경우 0 값을 반환하고, 오류로 인해 종료를 해야할 경우 0이 아닌 값들을 매게변수로 주는 것으로 알고 있습니다.
    // 코드 문맥을 봤을 때 비정상적인 종료로 인해 exit를 사용하므로 매게변수로 1을 넣어주는게 좋을 것 같습니다.

    /* 해당 포트 번호에 해당하는 서버의 듣기 소켓 식별자를 열어준다. */
    listenfd = Open_listenfd(argv[1]);

    /* 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit()호출 */
    while (1)
    { /*  pthread_create의 경우 argp 인자가 void*
    따라서 연결 식별자를 인자로 넣어줄 수 있게 안전하게 포인터를 만들어준다. */
        clientlen = sizeof(clientaddr);
        /* 클라이언트에게서 받은 연결 요청을 accept한다. connfd = proxy의 connfd*/
        connfdp = (int *)Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 포인터가 가리키는 값을 연결 식별자 값으로
        // connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        /* 연결이 성공했다는 메세지를 위해. Getnameinfo를 호출하면서 hostname과 port가 채워진다.*/
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        /*
           - 첫 번째 인자: 생성된 스레드의 식별자를 저장할 변수
           - 두 번째 인자: 스레드 특성 지정 (기본값 NULL 사용)
           - 세 번째 인자: 스레드가 실행할 함수 (thread 함수)
           - 네 번째 인자: 스레드 함수에 전달할 인자 (connfd 포인터, 클라이언트 소켓 식별자)
        */
        Pthread_create(&tid, NULL, thread, connfdp);
        // Pthread_create(&tid, NULL, thread, (void *)connfdp);
    }
}

/* 각 클라이언트 연결을 처리하는 쓰레드 함수, 쓰레드는 소켓 식별자를 받아와 해당 클라이언트와 통신
쓰레드는 자신을 분리하고, 동작이 끝난 후 소켓을 닫음
 */
void *thread(void *vargp)
{                                 // 스레드를 이용해서 동시성 구현
    int connfd = *((int *)vargp); // 인자로 소켓 식별자 받기
    // int connfd = ((int)vargp);

    Pthread_detach(pthread_self()); // 스레드 분리 (스레드가 종료되었을 때 자원 자동 해제)
    // 각각의 연결이 별도의 쓰레드에 의해서 독립적으로 처리 -> 서버가 명시적으로 각각의 피어 쓰레드 종료하는 것 불필요 -> detach
    // 메모리 누수를 방지하기 위해서 사용
    Free(vargp);   // 동적 할당한 파일 식별자 포인터를 free해준다.
    do_it(connfd); // 주요 로직 실행하기
    Close(connfd); // 로직이 끝나면 소켓 닫기
    return NULL;
}

/* 주요 로직 */
/* 클라이언트의 요청을 처리하는 함수, HTTP 헤더를 파싱하고, URI를 추출한다.
추출한 URI를 사용하여 서버로부터 데이터를 가져오는 send-request 함수를 호출한다. */
void do_it(int connfd)
{ /* 1. 클라이언트로부터 받은 요청의 헤더를 읽고 필요한 정보를 저장 */
    int clientfd;
    // 헤더 정보를 저장할 변수들
    char buf[MAXLINE], host[MAXLINE], port[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char uri_ptos[MAXLINE];
    rio_t rio;

    /* Read request line and headers from Client */
    // rio 라이브러리를 사용하여 입력 스트림 초기화
    Rio_readinitb(&rio, connfd); // rio 버퍼와 fd(proxy의 connfd)를 연결시켜준다.
    // 헤더라인을 읽음
    Rio_readlineb(&rio, buf, MAXLINE); // 그리고 rio(==proxy의 connfd)에 있는 한 줄(응답 라인)을 모두 buf로 옮긴다.
    printf("Request headers to proxy:\n");
    printf("%s", buf); //  읽은 헤더 라인을 출력
    // 헤더 라인을 파싱하여 method, uri, version을 추출
    sscanf(buf, "%s %s %s", method, uri, version); // buf에서 문자열 3개를 읽어와 각각 method, uri, version이라는 문자열에 저장

    /* Parse URI from GET request */
    // if (!(parse_uri(uri, uri_ptos, host, port)))
    //   return -1;
    parse_uri(uri, uri_ptos, host, port);

    clientfd = Open_clientfd(host, port);         // clientfd = proxy의 clientfd (연결됨)
    do_request(clientfd, method, uri_ptos, host); // clientfd에 Request headers 저장과 동시에 server의 connfd에 쓰여짐
    do_response(connfd, clientfd);
    Close(clientfd); // clientfd 역할 끝
}

/* do_request: proxy => server */
void do_request(int clientfd, char *method, char *uri_ptos, char *host)
{
    char buf[MAXLINE];
    printf("Request headers to server: \n");
    printf("%s %s %s\n", method, uri_ptos, new_version);

    /* Read request headers */
    sprintf(buf, "%s %s %s\r\n", method, uri_ptos, new_version); // GET /index.html HTTP/1.0
    sprintf(buf, "%sHost: %s\r\n", buf, host);                   // Host: www.google.com
    sprintf(buf, "%s%s\r\n", buf, user_agent_hdr);               // User-Agent: ~(bla bla)
    sprintf(buf, "%sConnections: close\r\n", buf);               // Connections: close
    sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf);      // Proxy-Connection: close

    /* Rio_writen: buf에서 clientfd로 strlen(buf) 바이트로 전송*/
    Rio_writen(clientfd, buf, (size_t)strlen(buf)); // => 적어주는 행위 자체가 요청하는거야~@!@!
}

/* do_response: server => proxy */
void do_response(int connfd, int clientfd)
{
    char buf[MAX_CACHE_SIZE];
    ssize_t n;
    rio_t rio;

    Rio_readinitb(&rio, clientfd);
    n = Rio_readnb(&rio, buf, MAX_CACHE_SIZE);
    Rio_writen(connfd, buf, n);
}

/* parse_uri: Parse URI, Proxy에서 Server로의 GET request를 하기 위해 필요 */
int parse_uri(char *uri, char *uri_ptos, char *host, char *port)
{
    char *ptr;

    /* 필요없는 http:// 부분 잘라서 host 추출 */
    if (!(ptr = strstr(uri, "://")))
        return -1; // ://가 없으면 unvalid uri
    ptr += 3;
    strcpy(host, ptr); // host = www.google.com:80\0     index.html

    /* uri_ptos(proxy => server로 보낼 uri) 추출 */
    if ((ptr = strchr(host, '/')))
    {
        *ptr = '\0'; // host = www.google.com:80
        ptr += 1;
        strcpy(uri_ptos, "/"); // uri_ptos = /
        strcat(uri_ptos, ptr); // uri_ptos = /index.html
    }
    else
        strcpy(uri_ptos, "/");

    /* port 추출 */
    if ((ptr = strchr(host, ':')))
    {                // host = www.google.com:80
        *ptr = '\0'; // host = www.google.com
        ptr += 1;
        strcpy(port, ptr); // port = 80
    }
    else
        strcpy(port, "80"); // port가 없을 경우 "80"을 넣어줌

    /*
    Before Parsing (Client로부터 받은 Request Line)
    => GET http://www.google.com:80/index.html HTTP/1.1

    Result Parsing (순차적으로 host, uri_ptos, port으로 파싱됨)
    => host = www.google.com
    => uri_ptos = /index.html
    => port = 80

    After Parsing (Server로 보낼 Request Line)
    => GET /index.html HTTP/11.
    */

    return 0; // function int return => for valid check
}