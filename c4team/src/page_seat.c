/*
 * page_seat.c  (담당: 류지언)  F4 좌석 예약·취소 + F6 시간대별 좌석 예약 화면
 * ------------------------------------------------------------------
 *   위쪽 왼편 : 좌석 버튼 12개 (초록 = 예약 가능, 주황 = 예약 중, 파랑 = 선택한 좌석)
 *   위쪽 오른편: 선택한 좌석, 학번 입력칸, [예약하기] [예약 취소] 버튼, 결과 안내
 *   아래쪽    : (선택 기능 F6) 시작·종료 시각을 골라 시간대 예약, 이 좌석의 예약 시간표
 *               (다른 학생의 학번은 뒤 4자리를 가린다)
 * 학생은 로그인한 학번으로 예약된다. 학번 입력칸은 관리자 모드에서만 보인다.
 * 예약 규칙 검사는 seat.c 가 하고, 이 파일은 입력을 읽고 결과를 보여준다.
 */
#include "library.h"
#include "ui.h"
#include <stdio.h>   /* swprintf */

/* 좌석 버튼 배치: 4칸 × 3줄 */
#define SEAT_COLUMNS  4
#define SEAT_W        92
#define SEAT_H        56
#define SEAT_GAP      10
#define PANEL_X       (CONTENT_X + 440)   /* 오른쪽 입력 영역이 시작하는 x 좌표 */
#define PANEL_WIDTH   (CONTENT_WIDTH - 440)

static HWND g_seat_buttons[SEAT_COUNT];
static HWND g_selected_label;
static HWND g_student_label;
static HWND g_student_edit;
static HWND g_seat_result;
static HWND g_empty_label;
static HWND g_start_combo;
static HWND g_end_combo;
static HWND g_slot_result;
static HWND g_slot_table;
static int g_selected_seat = 0;   /* 고른 좌석 번호 (0 이면 아직 고르지 않음) */

/* 고른 좌석의 시간대 예약 목록을 표에 채운다 */
static void fill_slot_table(void)
{
    const Seat *seat;
    int i;

    ui_table_clear(g_slot_table);
    if (g_selected_seat == 0) {
        return;
    }
    seat = &g_data.seats[g_selected_seat - 1];
    for (i = 0; i < seat->slot_count; i++) {
        wchar_t start_text[12];
        wchar_t end_text[12];
        wchar_t student_text[16];
        const wchar_t *cells[3];

        swprintf(start_text, 12, L"%d시", seat->slots[i].start_hour);
        swprintf(end_text, 12, L"%d시", seat->slots[i].end_hour);
        ui_student_text(seat->slots[i].student_id, student_text, 16);   /* 이용자 모드: 가림 */
        cells[0] = start_text;
        cells[1] = end_text;
        cells[2] = student_text;
        ui_table_add_row(g_slot_table, cells, 3);
    }
}

/* 좌석 버튼 12개를 다시 그리게 하고, 빈 좌석 수와 시간표를 새로 고친다 */
void seat_page_refresh(void)
{
    wchar_t text[40];
    int i;

    if (ui_current_page() == PAGE_SEAT) {
        ui_student_input_mode(g_student_label, g_student_edit);   /* ui.c */
    }
    for (i = 0; i < SEAT_COUNT; i++) {
        InvalidateRect(g_seat_buttons[i], NULL, TRUE);
    }
    swprintf(text, 40, L"빈 좌석 %d / %d석", count_empty_seats(g_data.seats), SEAT_COUNT);
    ui_set_label(g_empty_label, text, COLOR_TEXT);
    fill_slot_table();
}

/* 좌석 버튼을 눌렀을 때: 그 좌석을 "선택한 좌석"으로 만든다 */
static void select_seat(int seat_number)
{
    wchar_t text[30];

    g_selected_seat = seat_number;
    swprintf(text, 30, L"%02d번 좌석", seat_number);
    ui_set_label(g_selected_label, text, COLOR_BLUE);
    ui_set_label(g_seat_result, L"", COLOR_TEXT);
    ui_set_label(g_slot_result, L"", COLOR_TEXT);
    seat_page_refresh();
}

/* [예약하기] 버튼 (F4) */
static void do_reserve(void)
{
    int student_id = app_acting_student(g_student_edit);   /* main.c */
    wchar_t student_text[16];
    wchar_t text[100];
    Result result;

    if (student_id == -1) {   /* 로그인하지 않아서 로그인 화면으로 보냈다 */
        return;
    }
    if (g_selected_seat == 0) {
        ui_set_label(g_seat_result, L"먼저 왼쪽에서 좌석을 고르세요", COLOR_RED);
        return;
    }
    result = reserve_seat(g_data.seats, g_selected_seat, student_id,
                          g_data.users, g_data.user_count);   /* seat.c */

    ui_student_text(student_id, student_text, 16);
    swprintf(text, 100, L"예약 완료: %02d번 좌석 (학번 %ls)", g_selected_seat, student_text);
    ui_show_result(g_seat_result, result, text);
    if (result == RESULT_OK) {
        home_add_log(text);
        SetWindowTextW(g_student_edit, L"");   /* 관리자가 입력한 학번을 지운다 */
    }
    seat_page_refresh();
}

/* [예약 취소] 버튼 (F4) */
static void do_cancel(void)
{
    int student_id = app_acting_student(g_student_edit);   /* main.c */
    wchar_t student_text[16];
    wchar_t text[100];
    Result result;

    if (student_id == -1) {   /* 로그인하지 않아서 로그인 화면으로 보냈다 */
        return;
    }
    if (g_selected_seat == 0) {
        ui_set_label(g_seat_result, L"먼저 왼쪽에서 좌석을 고르세요", COLOR_RED);
        return;
    }
    result = cancel_seat(g_data.seats, g_selected_seat, student_id);   /* seat.c */

    ui_student_text(student_id, student_text, 16);
    swprintf(text, 100, L"예약 취소: %02d번 좌석 (학번 %ls)", g_selected_seat, student_text);
    ui_show_result(g_seat_result, result, text);
    if (result == RESULT_OK) {
        home_add_log(text);
        SetWindowTextW(g_student_edit, L"");   /* 관리자가 입력한 학번을 지운다 */
    }
    seat_page_refresh();
}

/* [시간대 예약] 버튼 (F6, 선택 기능) */
static void do_reserve_slot(void)
{
    /* 콤보박스에서 고른 칸 번호(0부터)를 시각으로 바꾼다 */
    int start_hour = OPEN_HOUR + (int)SendMessageW(g_start_combo, CB_GETCURSEL, 0, 0);
    int end_hour = OPEN_HOUR + 1 + (int)SendMessageW(g_end_combo, CB_GETCURSEL, 0, 0);
    int student_id = app_acting_student(g_student_edit);   /* main.c */
    wchar_t student_text[16];
    wchar_t text[100];
    Result result;

    if (student_id == -1) {
        return;
    }
    if (g_selected_seat == 0) {
        ui_set_label(g_slot_result, L"먼저 위에서 좌석을 고르세요", COLOR_RED);
        return;
    }
    result = reserve_time_slot(&g_data.seats[g_selected_seat - 1], start_hour, end_hour,
                               student_id, g_data.users, g_data.user_count);   /* seat.c */

    ui_student_text(student_id, student_text, 16);
    swprintf(text, 100, L"시간대 예약 완료: %02d번 좌석 %d시 ~ %d시 (학번 %ls)",
             g_selected_seat, start_hour, end_hour, student_text);
    ui_show_result(g_slot_result, result, text);
    if (result == RESULT_OK) {
        home_add_log(text);
        SetWindowTextW(g_student_edit, L"");   /* 관리자가 입력한 학번을 지운다 */
    }
    seat_page_refresh();
}

/* 좌석 화면의 컨트롤을 만든다 (프로그램 시작 시 한 번) */
void seat_page_create(void)
{
    wchar_t hour_text[10];
    int i;

    ui_label(PAGE_SEAT, L"좌석 예약", CONTENT_X, 28, 400, 40, FONT_TITLE, COLOR_TEXT);
    ui_label(PAGE_SEAT, L"좌석을 누르고 버튼을 누르세요. 로그인한 학번으로 예약됩니다. 아래에서 시간대 예약도 할 수 있어요.",
             CONTENT_X, 76, CONTENT_WIDTH, 24, FONT_NORMAL, COLOR_GRAY);

    /* 좌석 버튼 12개: i번째 좌석은 (i % 4)번째 칸, (i / 4)번째 줄 */
    for (i = 0; i < SEAT_COUNT; i++) {
        int x = CONTENT_X + (i % SEAT_COLUMNS) * (SEAT_W + SEAT_GAP);
        int y = 114 + (i / SEAT_COLUMNS) * (SEAT_H + SEAT_GAP);

        g_seat_buttons[i] = ui_color_button(PAGE_SEAT, ID_SEAT_FIRST + i, x, y, SEAT_W, SEAT_H);
    }
    ui_label(PAGE_SEAT, L"■ 예약 가능", CONTENT_X, 314, 110, 22, FONT_BOLD, COLOR_GREEN);
    ui_label(PAGE_SEAT, L"■ 예약 중", CONTENT_X + 120, 314, 100, 22, FONT_BOLD, COLOR_ORANGE);
    ui_label(PAGE_SEAT, L"■ 선택한 좌석", CONTENT_X + 230, 314, 140, 22, FONT_BOLD, COLOR_BLUE);
    g_empty_label = ui_label(PAGE_SEAT, L"", CONTENT_X, 340, 300, 22, FONT_BOLD, COLOR_TEXT);

    /* 오른쪽: 선택한 좌석과 학번 입력 */
    ui_label(PAGE_SEAT, L"선택한 좌석", PANEL_X, 114, PANEL_WIDTH, 22, FONT_BOLD, COLOR_GRAY);
    g_selected_label = ui_label(PAGE_SEAT, L"없음", PANEL_X, 138, PANEL_WIDTH, 40, FONT_TITLE,
                                COLOR_GRAY);
    g_student_label = ui_label(PAGE_SEAT, L"학번", PANEL_X, 190, PANEL_WIDTH, 22, FONT_BOLD,
                               COLOR_GRAY);
    g_student_edit = ui_edit(PAGE_SEAT, ID_SEAT_STUDENT_EDIT, PANEL_X, 214, PANEL_WIDTH, 32,
                             EDIT_NUMBER);
    ui_button(PAGE_SEAT, ID_SEAT_RESERVE, L"예약하기", PANEL_X, 262, 157, 40);
    ui_button(PAGE_SEAT, ID_SEAT_CANCEL, L"예약 취소", PANEL_X + 173, 262, 157, 40);
    g_seat_result = ui_label(PAGE_SEAT, L"", PANEL_X, 314, PANEL_WIDTH, 48, FONT_BOLD,
                             COLOR_TEXT);

    /* 아래쪽: F6 시간대 예약 (선택 기능) */
    ui_label(PAGE_SEAT, L"시간대 예약 (선택 기능)", CONTENT_X, 376, 400, 24, FONT_BOLD,
             COLOR_TEXT);
    ui_label(PAGE_SEAT, L"고른 좌석의 오늘 예약 시간표예요. 시간이 겹치는 예약은 할 수 없어요.",
             CONTENT_X, 400, CONTENT_WIDTH, 22, FONT_NORMAL, COLOR_GRAY);

    ui_label(PAGE_SEAT, L"시작", CONTENT_X, 436, 40, 24, FONT_BOLD, COLOR_GRAY);
    g_start_combo = ui_combo(PAGE_SEAT, ID_SLOT_START, CONTENT_X + 44, 432, 110, 300);
    ui_label(PAGE_SEAT, L"종료", CONTENT_X + 172, 436, 40, 24, FONT_BOLD, COLOR_GRAY);
    g_end_combo = ui_combo(PAGE_SEAT, ID_SLOT_END, CONTENT_X + 216, 432, 110, 300);
    ui_button(PAGE_SEAT, ID_SLOT_RESERVE, L"시간대 예약", CONTENT_X + 344, 430, 150, 34);

    /* 시작: 9시 ~ 17시, 종료: 10시 ~ 18시 */
    for (i = OPEN_HOUR; i < CLOSE_HOUR; i++) {
        swprintf(hour_text, 10, L"%d시", i);
        SendMessageW(g_start_combo, CB_ADDSTRING, 0, (LPARAM)hour_text);
        swprintf(hour_text, 10, L"%d시", i + 1);
        SendMessageW(g_end_combo, CB_ADDSTRING, 0, (LPARAM)hour_text);
    }
    SendMessageW(g_start_combo, CB_SETCURSEL, 0, 0);   /* 처음 선택: 9시 */
    SendMessageW(g_end_combo, CB_SETCURSEL, 0, 0);     /* 처음 선택: 10시 */

    g_slot_result = ui_label(PAGE_SEAT, L"", CONTENT_X, 472, CONTENT_WIDTH, 24, FONT_BOLD,
                             COLOR_TEXT);

    g_slot_table = ui_table(PAGE_SEAT, ID_SLOT_TABLE, CONTENT_X, 500, CONTENT_WIDTH, 116);
    ui_table_add_column(g_slot_table, 0, L"시작", 150);
    ui_table_add_column(g_slot_table, 1, L"종료", 150);
    ui_table_add_column(g_slot_table, 2, L"예약한 학번", 200);

    seat_page_refresh();
}

/* 로그아웃할 때: 다음 사람에게 보이지 않도록 선택한 좌석, 입력칸, 결과 안내를 지운다 */
void seat_page_reset(void)
{
    g_selected_seat = 0;
    SetWindowTextW(g_student_edit, L"");
    ui_set_label(g_selected_label, L"없음", COLOR_GRAY);
    ui_set_label(g_seat_result, L"", COLOR_TEXT);
    ui_set_label(g_slot_result, L"", COLOR_TEXT);
}

/* 버튼 클릭 처리 */
void seat_page_command(int id, int code)
{
    if (code != BN_CLICKED) {
        return;
    }
    if (id >= ID_SEAT_FIRST && id <= ID_SEAT_LAST) {
        select_seat(id - ID_SEAT_FIRST + 1);   /* 버튼 ID → 좌석 번호 */
    } else if (id == ID_SEAT_RESERVE) {
        do_reserve();
    } else if (id == ID_SEAT_CANCEL) {
        do_cancel();
    } else if (id == ID_SLOT_RESERVE) {
        do_reserve_slot();
    }
}

/* 좌석 버튼 하나를 그린다: 상태에 따라 색과 글자가 다르다 */
void seat_page_draw(const DRAWITEMSTRUCT *dis)
{
    int seat_number = (int)dis->CtlID - ID_SEAT_FIRST + 1;
    const Seat *seat = &g_data.seats[seat_number - 1];
    wchar_t text[40];
    COLORREF fill;
    COLORREF text_color;

    if (seat_number == g_selected_seat) {
        fill = COLOR_BLUE;
        text_color = COLOR_WHITE;
    } else if (seat->state == SEAT_RESERVED) {
        fill = COLOR_LIGHT_ORANGE;
        text_color = COLOR_ORANGE;
    } else {
        fill = COLOR_LIGHT_GREEN;
        text_color = COLOR_GREEN;
    }

    if (seat->state == SEAT_RESERVED) {
        swprintf(text, 40, L"%02d\n예약 중", seat_number);
    } else {
        swprintf(text, 40, L"%02d\n예약 가능", seat_number);
    }
    ui_draw_box(dis, COLOR_WHITE, fill, text_color, text);
}
