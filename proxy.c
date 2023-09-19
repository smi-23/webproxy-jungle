#include <stdio.h>
#include "csapp.h"

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

/* Function prototypes => 함수 순서 상관 X */
void do_it(int fb);
void do_request(int p_clientfd, char *method, char *uri_ptos, char *host);
void response(int p_connfd, int p_clientfd);
int parse_uri(char *uri, char *uri_ptos, char *host, char *port);
// int parse_responsehdrs(rio_t *rp, int length);

int main(int argc, char **argv) // arg 갯수와 arg배열을 가리키는 포인터
{
  int listenfd, p_connfd; // p_connfd = proxy_connfd
  char hostname[MAXLINE], port[MAXLINE]; // 프록시가 요청을 받고 응답해줄 클라이언트의 IP, Port
  socklen_t clientlen;
  struct sockaddr_storage clientadder;

  /* Check command line args */
  if (argc != 2){
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // argc가 두개 입력되지 않으면 에러
    exit(1);
  }

  /* 해당 포트 번호에 해당하는 듣기 소켓 식별자를 열어준다. */
  listenfd = Open_listendfd(argv[1]);

  /* 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 do_it() 호출 */
  while(1){
    clientlen = sizeof(clientadder);

    /* 클라이언트에게서 받은 연결 요청을 accept한다. p_connfd = proxy의 connfd */
    p_connfd = Accept(listenfd, (SA*)&clientadder, &clientlen);

    /* 연결이 성공했다는 메시지를 위해, Getnameinfo를 호출하면서 hostname과 port가 채워진다. */
    Getnameinfo((SA*)&clientadder, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    do_it(p_connfd);
    Close(p_connfd);
  }
  // printf("%s", user_agent_hdr);
  return 0;
}
