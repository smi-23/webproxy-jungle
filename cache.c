#include "cache.h"

/* 새로운 캐시를 생성하는 함수*/
cache *new_cache()
{
    // 캐시 구조체 포인터 생성 및 할당
    cache *c = malloc(sizeof(cache));
    // 캐시 초기화: 루트와 테일을 NULL로 설정하고 크기를 0으로 초기화
    c->root = NULL; // 캐시의 첫 번째 노드 포인터를 초기화
    c->tail = NULL; // 캐시의 마지막 노드 포인터를 초기화
    c->size = 0;    // 캐시의 크기를 0으로 초기화
    return c;       // 초기화된 캐시 구조체 포인터 반환
}

/* 새로운 캐시 노드를 생성하는 함수 */

// 노드를 동적으로 할당해주고 포인터 리턴
// 삭제할 때 각 포인터들도 지워야함 메모리 관리에 주의하기!
cache_node *new_cache_node(char *key, char *value)
{
    // 캐시 노드 구조체 포인터 생성 및 할당
    cache_node *node = malloc(sizeof(cache_node));
    // 키(key) 메모리 할당 및 복사
    node->key = malloc(strlen(key) + 1); // 키 문자열의 길이 + 1 크기의 메모리 할당 ,+1하는 이유 널문자 포함
    strcpy(node->key, key);              // 키 문자열을 복사하여 저장
    // 값(value) 메모리 할당 및 복사
    node->value = malloc(strlen(value) + 1); // 값 문자열의 길이 + 1 크기의 메모리 할당
    strcpy(node->value, value);              // 값 문자열을 복사하여 저장
    // 초기화된 캐시 노드 구조체 포인터 반환
    return node;
} // 이렇게 초기화된 캐시 노드는 캐시에 추가될 때 사용되며, 키와 값이 복사되어 보관

/* 동적할당된 노드를 반환하는 함수 */
/*
free_node 함수가 호출되면 먼저 캐시 노드에 할당된 키문자열 메모리를 free함수를 사용하여 해제,이것은해당 키 문자열이 동적으로 할당되었으므로 메모리 누수를 방지
다음으로, 캐시 노드에 할당된 값 문자열 메모리를 마찬가지로 free함수를 사용하여 해제, 값 문자열 역시 동적으로 할당되었으며,메모리를 반환하여 누수를 방지.
마지막으로 캐시노드 자체의 메모리를 free 함수를 사용하여 해제, 이것은 캐시 노드 구조체 자체의 메모리를 반환하는 단계
*/
/* 동적으로 할당된 캐시 노드를 해제하고 해당 메모리를 반환하는 함수 */
void free_node(cache_node *free_node)
{
    /* 먼저 캐시 노드에 할당된 키(key) 문자열 메모리를 해제 */
    free(free_node->key);

    /* 다음으로 캐시 노드에 할당된 값(value) 문자열 메모리를 해제 */
    free(free_node->value);

    /* 마지막으로 캐시 노드 자체의 메모리를 해제. */
    free(free_node);
}

/* 노드를 삭제하는 함수 */
void delete_node(cache *target_cache, cache_node *delete_node)
{
    // 1. 삭제 노드 prev랑 next를 연결한다.
    // 1.1 prev 노드의 next를 삭제 노드의 next로 변경
    if (delete_node->prev != NULL)
        delete_node->prev->next = delete_node->next;
    else
        target_cache->root = delete_node->next;
    // 1.2 next 노드의 prev를 삭제 노드의 prev로 변경
    if (delete_node->next != NULL)
        delete_node->next->prev = delete_node->prev;
    else
        target_cache->tail = delete_node->prev;

    // 2. 해당 노드를 free 해준다.(내부 필드를 포인터로 생성했기 때문에 프리해주는 함수를 따로 만듦)
    free_node(delete_node);
}

//  3. 데이터 찾기 (find_value)
//         1. key를 입력하면 데이터 찾기
//         2. 데이터를 찾으면 1 반환 (buf 값을 인자로 받고 넣어주면 데이터는 버퍼에 값 채우기)
//            이후 찾은 데이터를 삭제하고, 링크드 리스트 가장 앞에 넣어주자(LRU를 위해)
//         3. 데이터를 못 찾으면 0 반환
int find_cache(cache *target_cache, char *key, char *buf)
{
    cache_node *start = target_cache->root; // 시작 노드 설정
    while (start != NULL)
    {
        if (strcmp(start->key, key) == 0) // 같으면 0을 반환
        {                              // 같은 키 값을 찾으면?
            strcpy(buf, start->value); // 버퍼에 value 넣기
            // 이전에 있던 캐시를 삭제하고, 가장 앞에 넣는다.
            insert_cache(target_cache, start->key, start->value); // 찾은 값을 연결 리스트 가장 앞에 넣는다.
            delete_node(target_cache, start);                     // 기존 리스트는 지운다.
            return 1;
        }
        start = start->next;
    }
    return 0;
}

// 캐시가 잘 동작하는지 확인하기 위한 출력 함수
// int print_cache(cache *target_cache)
// {
//     cache_node *start = target_cache->root;          // 시작 노드 설정
//     printf("cache size = %d\n", target_cache->size); // 캐시 총 사이즈 출력
//     printf("cache root = %s\n", target_cache->root); // 캐시 시작 포인터 출력
//     printf("cache tail = %s\n", target_cache->tail); // 캐시 마지막 포인터 출력
//     printf("==============\n");
//     while (start != NULL)
//     { // 마지막 노드까지 값 출력하기
//         printf("node key = %s\n", start->key);
//         printf("node vaue = %s\n", start->value);
//         printf("==============\n");
//         start = start->next;
//     }
// }

// 4. 데이터 저장
//     1. 최대 오브젝트 사이즈 이하만 저장
//     2. 저장은 무조건 가장 앞에
//     3. 만약 넣으려는데 버퍼 사이즈 초과했다면?
//     4. 가장 오래 된 것을 찾아서 삭제하자
int insert_cache(cache *target_cache, char *key, char *value)
{
    printf("insert cache!\n");

    // 만약 최대 값을 넘어가면 읽은지 가장 오래된 노드를 제거한다.
    while (target_cache->size + strlen(value) > MAX_CACHE_SIZE)
    { // 넣을 수 있는 사이즈가 될 때까지 반복
        delete_node(target_cache, target_cache->tail);
    }
    // 값이 추가 되기 때문에 총 사이즈를 늘린다.
    target_cache->size += strlen(value); // 사이즈를 키운다.

    // 1. 새로운 노드 생성하기
    cache_node *new_node = new_cache_node(key, value);
    // 2. 노드를 캐시에 추가한다.
    // 2.1. 새로운 노드의 next 포인터를 최선 노드로 한다.
    new_node->next = target_cache->root;
    // 2.2. 최선 노드에 prev 포인터가 새로운 노드를 가리키도록 한다.
    // 만약 root가 널이면?
    if (target_cache->root != NULL)
        target_cache->root->prev = new_node;
    else
        target_cache->tail = new_node;
    // 2.3. 캐시의 루트 포인터가 새로운 노드를 가리키도록 한다.
    target_cache->root = new_node;
}