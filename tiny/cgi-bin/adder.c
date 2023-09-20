#include "csapp.h"
#include "stdio.h"

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* 웹 브라우저로 부터 쿼리 문자열을 추출 */
  if ((buf = getenv("QUERY_STRING")) != NULL) // getenv함수를 사용하여 웹브라우저로부터 전달된 쿼리 문자열을 buf에 저장
  {
    /*쿼리 문자열을 '&'로 나눈다.*/
    /*http://www.example.com/search?query=apple&category=fruits&page=1
    여기서 쿼리 문자열은 query=apple&category=fruits&page=1
*/
    p = strchr(buf, '&');
    *p = '\0';
    // strcpy(arg1, buf);
    // strcpy(arg2, p+1);
    // n1 = atoi(arg1);
    // n2 = atoi(arg2);
    /*
    *p = '\0'; 라인은 쿼리 문자열을 두 부분으로 나누기 위해 사용됩니다.
    웹 서버는 & 문자를 사용하여 각 매개변수와 값을 구분하므로, 이 부분을 나누기 위해 & 문자를 '\0' (널) 문자로 대체합니다.
    이렇게 하면 buf 문자열은 첫 번째 매개변수와 그 값을 가리키고, p + 1은 두 번째 매개변수와 그 값을 가리킵니다.
    */
    sscanf(buf, "n1=%d", &n1);
    sscanf(p + 1, "n2=%d", &n2);
  }

  /* 응답 본문을 생성*/
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* HTTP응답을 생성 */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  /*응답 본문을 출력하고 출력 버퍼를 비운다.*/
  printf("%s", content);
  fflush(stdout);
  /*프로그램 종료*/
  exit(0);
}