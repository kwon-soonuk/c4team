/*
 * page_search.c  (담당: 송영준)  F2 도서 검색 화면
 * ------------------------------------------------------------------
 *   - 검색어 입력칸 + [검색] 버튼
 *   - 결과 표: 도서 번호 / 제목 / 저자 / 상태
 *   - 표에서 도서를 두 번 클릭하면 그 도서 번호를 가지고 대출·반납 화면으로 이동
 * 실제 검색 계산은 search.c 의 search_books() 가 하고, 이 파일은 보여주기만 한다.
 */
#include "library.h"
#include "ui.h"
#include <commctrl.h>   /* NM_DBLCLK */
#include <stdio.h>      /* swprintf */

#define MAX_RESULTS  500   /* 한 번에 보여줄 검색 결과 최대 수 */
#define KEYWORD_LEN  100   /* 검색어 최대 바이트 수 (UTF-8) */

static HWND g_keyword_edit;
static HWND g_count_label;
static HWND g_table;
static int g_found[MAX_RESULTS];      /* 표의 n번째 줄 = books 배열의 g_found[n]번 칸 */
static int g_found_count = 0;
static char g_keyword[KEYWORD_LEN] = "";   /* 마지막 검색어 (처음에는 빈 문자열 = 전체) */

/* 마지막 검색어로 다시 검색해서 표를 채운다 */
static void run_search(void)
{
    wchar_t message[60];
    int i;

    g_found_count = search_books(g_data.books, g_data.book_count, g_keyword,
                                 g_found, MAX_RESULTS);

    ui_table_clear(g_table);
    for (i = 0; i < g_found_count; i++) {
        const Book *book = &g_data.books[g_found[i]];
        wchar_t id_text[12];
        wchar_t title[TITLE_LEN];
        wchar_t author[AUTHOR_LEN];
        const wchar_t *cells[4];

        swprintf(id_text, 12, L"%d", book->id);
        ui_utf8_to_wide(book->title, title, TITLE_LEN);     /* 파일의 UTF-8 → 화면용 */
        ui_utf8_to_wide(book->author, author, AUTHOR_LEN);

        cells[0] = id_text;
        cells[1] = title;
        cells[2] = author;
        if (book->state == BOOK_AVAILABLE) {
            cells[3] = L"대출 가능";
        } else {
            cells[3] = L"대출 중";
        }
        ui_table_add_row(g_table, cells, 4);
    }

    if (g_found_count == 0) {
        ui_set_label(g_count_label, L"검색 결과가 없습니다", COLOR_RED);
    } else {
        swprintf(message, 60, L"검색 결과 %d권", g_found_count);
        ui_set_label(g_count_label, message, COLOR_TEXT);
    }
}

/* 검색 화면의 컨트롤을 만든다 (프로그램 시작 시 한 번) */
void search_page_create(void)
{
    ui_label(PAGE_SEARCH, L"도서 검색", CONTENT_X, 28, 400, 40, FONT_TITLE, COLOR_TEXT);
    ui_label(PAGE_SEARCH, L"제목의 일부만 입력해도 찾을 수 있어요. 비워 두고 검색하면 전체 목록이 나와요.",
             CONTENT_X, 76, CONTENT_WIDTH, 24, FONT_NORMAL, COLOR_GRAY);

    g_keyword_edit = ui_edit(PAGE_SEARCH, ID_SEARCH_EDIT, CONTENT_X, 116, 620, 32, EDIT_TEXT);
    ui_button(PAGE_SEARCH, ID_SEARCH_BUTTON, L"검색", CONTENT_X + 635, 116, 135, 32);

    g_count_label = ui_label(PAGE_SEARCH, L"", CONTENT_X, 166, 400, 24, FONT_BOLD, COLOR_TEXT);
    g_table = ui_table(PAGE_SEARCH, ID_SEARCH_TABLE, CONTENT_X, 196, CONTENT_WIDTH, 380);
    ui_table_add_column(g_table, 0, L"도서 번호", 100);
    ui_table_add_column(g_table, 1, L"제목", 320);
    ui_table_add_column(g_table, 2, L"저자", 180);
    ui_table_add_column(g_table, 3, L"상태", 140);

    ui_label(PAGE_SEARCH, L"표에서 도서를 두 번 클릭하면 대출 · 반납 화면으로 이동합니다",
             CONTENT_X, 588, CONTENT_WIDTH, 24, FONT_NORMAL, COLOR_GRAY);

    run_search();   /* 처음에는 전체 목록을 보여준다 */
}

/* 다른 화면에서 대출·반납을 했을 수 있으니, 상태를 최신으로 다시 보여준다 */
void search_page_refresh(void)
{
    run_search();
}

/* [검색] 버튼 (검색 화면에서 Enter 키를 눌러도 main.c 가 이 함수를 부른다) */
void search_page_command(int id, int code)
{
    if (id == ID_SEARCH_BUTTON && code == BN_CLICKED) {
        ui_get_text(g_keyword_edit, g_keyword, KEYWORD_LEN);   /* 입력칸 → UTF-8 검색어 */
        run_search();
    }
}

/* 표에서 온 알림: 두 번 클릭하면 대출·반납 화면으로 이동 */
void search_page_notify(const NMHDR *header)
{
    int row;

    if ((int)header->idFrom != ID_SEARCH_TABLE || header->code != NM_DBLCLK) {
        return;
    }
    row = ui_table_selected_row(g_table);
    if (row >= 0 && row < g_found_count) {
        loan_page_set_book(g_data.books[g_found[row]].id);   /* page_loan.c 의 함수 */
        app_go_to_page(PAGE_LOAN);
    }
}
