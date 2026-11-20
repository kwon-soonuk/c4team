/*
 * page_home.c  (담당: 권순욱)  F1 메인 화면(홈)
 * ------------------------------------------------------------------
 *   - 카드 3개: 전체 도서 수 / 대출 중인 도서 수 / 빈 좌석 수
 *               카드를 누르면 해당 화면으로 이동한다
 *   - 카드 아래쪽은 지금 누가 쓰고 있는지에 따라 다르게 보인다
 *       로그인 안 함 : 이용 안내
 *       학생 로그인  : 내 대출 도서 표, 내 좌석과 시간대 예약
 *       관리자 모드  : 최근 처리 내역 (학생에게는 보이지 않는다)
 *   - 다른 화면에서 대출·반납·예약에 성공하면 home_add_log() 로 처리 내역에 한 줄씩 추가한다
 */
#include "library.h"
#include "ui.h"
#include <commctrl.h> /* LVM_ENSUREVISIBLE */
#include <stdio.h>    /* swprintf */
#include <wchar.h>    /* wcscpy, wcscat */

#define TILE_COUNT 3

static HWND g_tiles[TILE_COUNT];   /* 카드(색 버튼) 3개 */
static HWND g_notice_label;        /* [로그인 안 함] 알림 한 줄 (예: 자동 로그아웃 안내) */
static HWND g_guide_label;         /* [로그인 안 함] 이용 안내 */
static HWND g_my_label;            /* [학생] "내 대출 도서 (n / 10권)" */
static HWND g_my_table;            /* [학생] 내 대출 도서 표 */
static HWND g_my_seat_label;       /* [학생] 내 좌석, 시간대 예약 */
static HWND g_log_label;           /* [관리자] "최근 처리 내역" */
static HWND g_log_table;           /* [관리자] 최근 처리 내역 표 */

/* 홈 화면의 컨트롤을 만든다 (프로그램 시작 시 한 번) */
void home_create(void)
{
    int i;

    ui_label(PAGE_HOME, L"홈", CONTENT_X, 28, 400, 40, FONT_TITLE, COLOR_TEXT);
    ui_label(PAGE_HOME, L"카드를 누르거나 왼쪽 메뉴에서 원하는 기능을 고르세요",
             CONTENT_X, 76, CONTENT_WIDTH, 24, FONT_NORMAL, COLOR_GRAY);

    /* 카드 3개를 가로로 나란히: 너비 243, 간격 20 */
    for (i = 0; i < TILE_COUNT; i++) {
        g_tiles[i] = ui_color_button(PAGE_HOME, ID_HOME_TILE_SEARCH + i,
                                     CONTENT_X + i * 263, 118, 243, 150);
    }

    /* 아래쪽 세 묶음은 같은 자리에 겹쳐 만들고, home_refresh() 에서 하나만 보이게 한다 */

    /* (1) 로그인 안 함: 알림 한 줄 + 이용 안내 */
    g_notice_label = ui_label(PAGE_HOME, L"", CONTENT_X, 292, CONTENT_WIDTH, 26, FONT_BOLD,
                              COLOR_BLUE);
    g_guide_label = ui_label(PAGE_HOME,
        L"도서 검색과 좌석 현황은 누구나 볼 수 있어요.\n"
        L"대출 · 반납 · 좌석 예약은 왼쪽 메뉴의 [학생 로그인]을 한 뒤에 이용할 수 있어요.\n\n"
        L"처음 이용하는 학생은 조교(관리자)에게 학번 등록을 요청하고 임시 비밀번호를 받으세요.\n"
        L"임시 비밀번호로 로그인하면 새 비밀번호로 바꾼 뒤에 이용할 수 있어요.",
        CONTENT_X, 328, CONTENT_WIDTH, 140, FONT_NORMAL, COLOR_GRAY);

    /* (2) 학생 로그인: 내 대출 도서, 내 좌석 */
    g_my_label = ui_label(PAGE_HOME, L"", CONTENT_X, 288, 400, 26, FONT_BOLD, COLOR_TEXT);
    g_my_table = ui_table(PAGE_HOME, ID_HOME_MY_TABLE, CONTENT_X, 318, CONTENT_WIDTH, 200);
    ui_table_add_column(g_my_table, 0, L"도서 번호", 100);
    ui_table_add_column(g_my_table, 1, L"제목", 400);
    ui_table_add_column(g_my_table, 2, L"저자", 200);
    g_my_seat_label = ui_label(PAGE_HOME, L"", CONTENT_X, 532, CONTENT_WIDTH, 80, FONT_NORMAL,
                               COLOR_TEXT);

    /* (3) 관리자 모드: 최근 처리 내역 */
    g_log_label = ui_label(PAGE_HOME, L"최근 처리 내역 (관리자만 볼 수 있어요)", CONTENT_X, 288, 400,
                           26, FONT_BOLD, COLOR_TEXT);
    g_log_table = ui_table(PAGE_HOME, ID_HOME_LOG, CONTENT_X, 318, CONTENT_WIDTH, 296);
    ui_table_add_column(g_log_table, 0, L"시각", 110);
    ui_table_add_column(g_log_table, 1, L"내용", 630);
}

/* [학생] 내 대출 도서 표와 내 좌석 글을 채운다 */
static void fill_my_info(void)
{
    wchar_t text[300];
    wchar_t part[40];
    int my_seat = find_user_seat(&g_data, g_student_id);   /* admin.c */
    int count = 0;
    int i, j;

    ui_table_clear(g_my_table);
    for (i = 0; i < g_data.book_count; i++) {
        const Book *book = &g_data.books[i];

        if (book->state == BOOK_BORROWED && book->borrower_id == g_student_id) {
            wchar_t id_text[12];
            wchar_t title[TITLE_LEN];
            wchar_t author[AUTHOR_LEN];
            const wchar_t *cells[3];

            swprintf(id_text, 12, L"%d", book->id);
            ui_utf8_to_wide(book->title, title, TITLE_LEN);
            ui_utf8_to_wide(book->author, author, AUTHOR_LEN);
            cells[0] = id_text;
            cells[1] = title;
            cells[2] = author;
            ui_table_add_row(g_my_table, cells, 3);
            count++;
        }
    }
    swprintf(text, 300, L"내 대출 도서 (%d / %d권)", count, MAX_LOANS);
    ui_set_label(g_my_label, text, COLOR_TEXT);

    /* 내 좌석과 시간대 예약을 한 문장으로 이어 붙인다 */
    if (my_seat == 0) {
        swprintf(text, 300, L"내 좌석: 없음\n내 시간대 예약:");
    } else {
        swprintf(text, 300, L"내 좌석: %02d번\n내 시간대 예약:", my_seat);
    }
    count = 0;
    for (i = 0; i < SEAT_COUNT; i++) {
        for (j = 0; j < g_data.seats[i].slot_count; j++) {
            const TimeSlot *slot = &g_data.seats[i].slots[j];

            if (slot->student_id == g_student_id && count < 6) {   /* 화면에는 6건까지만 */
                swprintf(part, 40, L"  %02d번 %d~%d시", g_data.seats[i].number, slot->start_hour,
                         slot->end_hour);
                wcscat(text, part);   /* 문자열 뒤에 이어 붙이기 (wchar_t 용 strcat) */
                count++;
            }
        }
    }
    if (count == 0) {
        wcscat(text, L" 없음");
    }
    ui_set_label(g_my_seat_label, text, COLOR_TEXT);
}

/* 컨트롤 하나를 보이거나(1) 숨긴다(0) */
static void set_visible(HWND control, int visible)
{
    if (visible) {
        ShowWindow(control, SW_SHOW);
    } else {
        ShowWindow(control, SW_HIDE);
    }
}

/* 카드 숫자를 다시 그리고, 아래쪽을 지금 상태(로그인 안 함 / 학생 / 관리자)에 맞게 바꾼다 */
void home_refresh(void)
{
    int is_student = (!g_is_admin && g_student_id != 0);
    int is_guest = (!g_is_admin && g_student_id == 0);
    int i;

    for (i = 0; i < TILE_COUNT; i++) {
        InvalidateRect(g_tiles[i], NULL, TRUE);
    }

    set_visible(g_notice_label, is_guest);
    set_visible(g_guide_label, is_guest);
    set_visible(g_my_label, is_student);
    set_visible(g_my_table, is_student);
    set_visible(g_my_seat_label, is_student);
    set_visible(g_log_label, g_is_admin);
    set_visible(g_log_table, g_is_admin);

    if (is_student) {
        fill_my_info();
    }
}

/* 홈 화면(로그인 안 한 상태)에 보여줄 알림 한 줄을 정한다. 빈 문자열이면 지운다 */
void home_set_notice(const wchar_t *text)
{
    ui_set_label(g_notice_label, text, COLOR_BLUE);
}

/* 카드를 누르면 해당 화면으로 이동 */
void home_command(int id, int code)
{
    if (code != BN_CLICKED) {
        return;
    }
    if (id == ID_HOME_TILE_SEARCH) {
        app_go_to_page(PAGE_SEARCH);
    } else if (id == ID_HOME_TILE_LOAN) {
        app_go_to_page(PAGE_LOAN);
    } else if (id == ID_HOME_TILE_SEAT) {
        app_go_to_page(PAGE_SEAT);
    }
}

/* 카드 하나를 그린다: 제목(작은 글씨) + 숫자(큰 글씨) + 안내 */
void home_draw(const DRAWITEMSTRUCT *dis)
{
    wchar_t title[30];
    wchar_t value[30];
    COLORREF value_color;
    COLORREF fill = COLOR_LIGHT_BLUE;
    RECT rect = dis->rcItem;
    RECT title_rect, value_rect, hint_rect;
    int id = (int)dis->CtlID;

    if (id == ID_HOME_TILE_SEARCH) {
        wcscpy(title, L"도서 검색");
        swprintf(value, 30, L"전체 %d권", g_data.book_count);
        value_color = COLOR_BLUE;
    } else if (id == ID_HOME_TILE_LOAN) {
        wcscpy(title, L"대출 · 반납");
        swprintf(value, 30, L"대출 중 %d권",
                 count_borrowed_books(g_data.books, g_data.book_count));
        value_color = COLOR_ORANGE;
    } else {
        wcscpy(title, L"좌석 예약");
        swprintf(value, 30, L"빈 좌석 %d/%d", count_empty_seats(g_data.seats), SEAT_COUNT);
        value_color = COLOR_GREEN;
    }

    if (dis->itemState & ODS_SELECTED) {   /* 누르고 있는 동안 조금 진하게 */
        fill = RGB(210, 225, 255);
    }
    ui_fill_round_rect(dis->hDC, &rect, COLOR_WHITE, fill);

    title_rect = rect;
    title_rect.left += 22;
    title_rect.top += 20;
    ui_draw_text(dis->hDC, title, &title_rect, FONT_BOLD, COLOR_BLUE, DT_LEFT | DT_SINGLELINE);

    value_rect = rect;
    value_rect.left += 22;
    value_rect.top += 56;
    ui_draw_text(dis->hDC, value, &value_rect, FONT_TITLE, value_color, DT_LEFT | DT_SINGLELINE);

    hint_rect = rect;
    hint_rect.left += 22;
    hint_rect.top += 112;
    ui_draw_text(dis->hDC, L"눌러서 열기  >", &hint_rect, FONT_NORMAL, COLOR_GRAY,
                 DT_LEFT | DT_SINGLELINE);
}

/* 처리 내역에 "지금 시각 + 내용" 한 줄을 추가한다 (다른 화면에서도 호출). 표는 관리자만 볼 수 있다 */
void home_add_log(const wchar_t *text)
{
    SYSTEMTIME now;
    wchar_t time_text[20];
    const wchar_t *cells[2];
    int row;

    GetLocalTime(&now);   /* 컴퓨터의 현재 시각 */
    swprintf(time_text, 20, L"%02d:%02d:%02d", (int)now.wHour, (int)now.wMinute,
             (int)now.wSecond);

    cells[0] = time_text;
    cells[1] = text;
    row = ui_table_add_row(g_log_table, cells, 2);
    SendMessageW(g_log_table, LVM_ENSUREVISIBLE, (WPARAM)row, FALSE);   /* 새 줄이 보이게 스크롤 */
}
