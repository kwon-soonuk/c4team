/*
 * search.c  (담당: 송영준)  F2 도서 검색
 * ------------------------------------------------------------------
 * 화면과 상관없는 "검색 계산"만 한다.
 * 찾은 결과를 표에 어떻게 보여줄지는 page_search.c 가 정한다.
 */
#include "library.h"
#include <string.h>   /* strstr */

/*
 * 제목에 keyword 가 들어 있는 도서를 모두 찾는다.
 *   found     : 찾은 도서의 "배열 위치(인덱스)"를 차례로 저장할 배열
 *   max_found : found 배열에 넣을 수 있는 최대 개수
 *   반환값    : 찾은 도서 수 (keyword 가 빈 문자열이면 모든 도서가 찾아진다)
 *
 * 예) 제목이 "C언어 기초", "C언어 실습", "자료구조" 이고 keyword 가 "C언어" 이면
 *     found = {0, 1}, 반환값 = 2
 */
int search_books(const Book books[], int book_count, const char *keyword,
                 int found[], int max_found)
{
    int i;
    int found_count = 0;

    for (i = 0; i < book_count && found_count < max_found; i++) {
        /* strstr(긴 문자열, 찾을 문자열): 들어 있으면 그 위치, 없으면 NULL */
        if (strstr(books[i].title, keyword) != NULL) {
            found[found_count] = i;
            found_count++;
        }
    }
    return found_count;
}

/*
 * 도서 번호로 배열 위치(인덱스)를 찾는다. 없으면 -1 을 돌려준다.
 * 대출·반납(loan.c)과 화면(page_loan.c)에서도 가져다 쓴다.
 */
int find_book_index(const Book books[], int book_count, int book_id)
{
    int i;

    for (i = 0; i < book_count; i++) {
        if (books[i].id == book_id) {
            return i;
        }
    }
    return -1;
}
