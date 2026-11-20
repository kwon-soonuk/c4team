/*
 * page_loan.c  (담당: 유강훈)  F3 도서 대출·반납 화면
 * ------------------------------------------------------------------
 *   왼쪽 : 도서 번호 입력칸, [대출] [반납] 버튼, 결과 안내, 도서 정보
 *          학생은 로그인한 학번으로 처리된다. 학번 입력칸은 관리자 모드에서만 보인다
 *   오른쪽: 현재 대출 중인 도서 표 (다른 학생의 학번은 뒤 4자리를 가린다)
 * 대출·반납 규칙 검사는 loan.c 가 하고, 이 파일은 입력을 읽고 결과를 보여준다.
 */
#include "library.h"
#include "ui.h"
#include <stdio.h>   /* swprintf */

#define LEFT_WIDTH  360               /* 왼쪽 입력 영역 너비 */
#define RIGHT_X     (CONTENT_X + 390) /* 오른쪽 표가 시작하는 x 좌표 */
#define RIGHT_WIDTH (CONTENT_WIDTH - 390)

static HWND g_book_edit;
static HWND g_student_label;
static HWND g_student_edit;
static HWND g_result_label;
static HWND g_info_label;
static HWND g_table;

/* 도서 번호 입력칸에 맞는 도서 정보(제목, 상태)를 보여준다 */
static void show_book_info(void)
{
    wchar_t text[200];
    wchar_t title[TITLE_LEN];
    wchar_t student_text[16];
    int book_id = ui_get_number(g_book_edit);
    int index = find_book_index(g_data.books, g_data.book_count, book_id);   /* search.c */
    const Book *book;

    if (book_id == 0) {
        ui_set_label(g_info_label, L"도서 번호를 입력하면 여기에 도서 정보가 보여요", COLOR_GRAY);
        return;
    }
    if (index == -1) {
        ui_set_label(g_info_label, L"없는 도서 번호입니다", COLOR_RED);
        return;
    }

    book = &g_data.books[index];
    ui_utf8_to_wide(book->title, title, TITLE_LEN);
    if (book->state == BOOK_AVAILABLE) {
        swprintf(text, 200, L"%ls\n상태: 대출 가능", title);
    } else {
        ui_student_text(book->borrower_id, student_text, 16);   /* 이용자 모드에서는 뒤 4자리를 가림 */
        swprintf(text, 200, L"%ls\n상태: 대출 중 (학번 %ls)", title, student_text);
    }
    ui_set_label(g_info_label, text, COLOR_TEXT);
}

/* "대출 완료: [101] C언어 기초 (학번 20261234)" 같은 성공 문구를 만든다 */
static void make_success_text(wchar_t *text, int text_count, const wchar_t *action,
                              int book_id, int student_id)
{
    wchar_t title[TITLE_LEN] = L"";
    wchar_t student_text[16];
    int index = find_book_index(g_data.books, g_data.book_count, book_id);

    if (index != -1) {
        ui_utf8_to_wide(g_data.books[index].title, title, TITLE_LEN);
    }
    ui_student_text(student_id, student_text, 16);
    swprintf(text, text_count, L"%ls 완료: [%d] %ls (학번 %ls)", action, book_id, title,
             student_text);
}

/* [대출] 버튼 */
static void do_borrow(void)
{
    int book_id = ui_get_number(g_book_edit);
    int student_id = app_acting_student(g_student_edit);   /* main.c : 로그인한 학번(관리자는 입력칸) */
    wchar_t text[200];
    Result result;

    if (student_id == -1) {   /* 로그인하지 않아서 로그인 화면으로 보냈다 */
        return;
    }
    result = borrow_book(g_data.books, g_data.book_count, book_id, student_id,
                         g_data.users, g_data.user_count);   /* loan.c */

    make_success_text(text, 200, L"대출", book_id, student_id);
    ui_show_result(g_result_label, result, text);
    if (result == RESULT_OK) {
        home_add_log(text);   /* 처리 내역에 기록 (관리자만 볼 수 있다) */
        SetWindowTextW(g_student_edit, L"");   /* 관리자가 입력한 학번을 지운다 */
    }
    loan_page_refresh();
}

/* [반납] 버튼 */
static void do_return(void)
{
    int book_id = ui_get_number(g_book_edit);
    int student_id = app_acting_student(g_student_edit);
    wchar_t text[200];
    Result result;

    if (student_id == -1) {
        return;
    }
    result = return_book(g_data.books, g_data.book_count, book_id, student_id);   /* loan.c */

    make_success_text(text, 200, L"반납", book_id, student_id);
    ui_show_result(g_result_label, result, text);
    if (result == RESULT_OK) {
        home_add_log(text);
        SetWindowTextW(g_student_edit, L"");
    }
    loan_page_refresh();
}

/* 대출·반납 화면의 컨트롤을 만든다 (프로그램 시작 시 한 번) */
void loan_page_create(void)
{
    ui_label(PAGE_LOAN, L"대출 · 반납", CONTENT_X, 28, 400, 40, FONT_TITLE, COLOR_TEXT);
    ui_label(PAGE_LOAN, L"도서 번호를 입력하고 버튼을 누르세요. 로그인한 학번으로 처리됩니다",
             CONTENT_X, 76, CONTENT_WIDTH, 24, FONT_NORMAL, COLOR_GRAY);

    /* 왼쪽: 입력 영역 */
    ui_label(PAGE_LOAN, L"도서 번호", CONTENT_X, 118, LEFT_WIDTH, 22, FONT_BOLD, COLOR_GRAY);
    g_book_edit = ui_edit(PAGE_LOAN, ID_LOAN_BOOK_EDIT, CONTENT_X, 142, LEFT_WIDTH, 32,
                          EDIT_NUMBER);
    g_student_label = ui_label(PAGE_LOAN, L"학번", CONTENT_X, 190, LEFT_WIDTH, 22, FONT_BOLD,
                               COLOR_GRAY);
    g_student_edit = ui_edit(PAGE_LOAN, ID_LOAN_STUDENT_EDIT, CONTENT_X, 214, LEFT_WIDTH, 32,
                             EDIT_NUMBER);

    ui_button(PAGE_LOAN, ID_LOAN_BORROW, L"대출", CONTENT_X, 264, 172, 40);
    ui_button(PAGE_LOAN, ID_LOAN_RETURN, L"반납", CONTENT_X + 188, 264, 172, 40);

    g_result_label = ui_label(PAGE_LOAN, L"", CONTENT_X, 316, LEFT_WIDTH, 66, FONT_BOLD,
                              COLOR_TEXT);

    ui_label(PAGE_LOAN, L"도서 정보", CONTENT_X, 398, LEFT_WIDTH, 22, FONT_BOLD, COLOR_GRAY);
    g_info_label = ui_label(PAGE_LOAN, L"", CONTENT_X, 422, LEFT_WIDTH, 66, FONT_NORMAL,
                            COLOR_TEXT);

    /* 오른쪽: 대출 중인 도서 표 */
    ui_label(PAGE_LOAN, L"현재 대출 중인 도서", RIGHT_X, 118, RIGHT_WIDTH, 22, FONT_BOLD,
             COLOR_GRAY);
    g_table = ui_table(PAGE_LOAN, ID_LOAN_TABLE, RIGHT_X, 142, RIGHT_WIDTH, 470);
    ui_table_add_column(g_table, 0, L"번호", 60);
    ui_table_add_column(g_table, 1, L"제목", 200);
    ui_table_add_column(g_table, 2, L"학번", 100);

    loan_page_refresh();
}

/* 대출 중인 도서 표와 도서 정보를 최신으로 다시 채우고, 학번 입력칸을 지금 상태에 맞춘다 */
void loan_page_refresh(void)
{
    int i;

    if (ui_current_page() == PAGE_LOAN) {
        ui_student_input_mode(g_student_label, g_student_edit);   /* ui.c */
    }

    ui_table_clear(g_table);
    for (i = 0; i < g_data.book_count; i++) {
        const Book *book = &g_data.books[i];

        if (book->state == BOOK_BORROWED) {
            wchar_t id_text[12];
            wchar_t title[TITLE_LEN];
            wchar_t student_text[16];
            const wchar_t *cells[3];

            swprintf(id_text, 12, L"%d", book->id);
            ui_utf8_to_wide(book->title, title, TITLE_LEN);
            ui_student_text(book->borrower_id, student_text, 16);
            cells[0] = id_text;
            cells[1] = title;
            cells[2] = student_text;
            ui_table_add_row(g_table, cells, 3);
        }
    }
    show_book_info();
}

/* 버튼 클릭과 입력칸 변경 처리 */
void loan_page_command(int id, int code)
{
    if (id == ID_LOAN_BOOK_EDIT && code == EN_CHANGE) {   /* 도서 번호를 입력할 때마다 */
        show_book_info();
        return;
    }
    if (code != BN_CLICKED) {
        return;
    }
    if (id == ID_LOAN_BORROW) {
        do_borrow();
    } else if (id == ID_LOAN_RETURN) {
        do_return();
    }
}

/* 검색 화면에서 도서를 두 번 클릭했을 때: 도서 번호를 미리 채워 둔다 */
void loan_page_set_book(int book_id)
{
    ui_set_number(g_book_edit, book_id);   /* 글자가 바뀌면 EN_CHANGE 로 도서 정보도 갱신 */
    ui_set_label(g_result_label, L"[대출] 또는 [반납]을 누르세요", COLOR_GRAY);
}

/* 로그아웃할 때: 다음 사람에게 보이지 않도록 입력칸과 결과 안내를 지운다 */
void loan_page_reset(void)
{
    SetWindowTextW(g_book_edit, L"");
    SetWindowTextW(g_student_edit, L"");
    ui_set_label(g_result_label, L"", COLOR_TEXT);
}
