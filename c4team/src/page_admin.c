/*
 * page_admin.c  (담당: 송영준)  F7 관리자 로그인 화면 + 관리자 화면
 * ------------------------------------------------------------------
 * 화면이 두 개다.
 *   PAGE_LOGIN : 비밀번호 입력칸과 [로그인] 버튼 (로그인하지 않았을 때)
 *   PAGE_ADMIN : 도서 관리, 학생 계정 관리(등록·삭제·비밀번호 초기화), 좌석 관리, 비밀번호 변경,
 *                등록 학번 현황 표, 시간대 예약 현황 표(고른 예약 취소)
 * 로그인은 5번 연속으로 틀리면 10초 동안 막히고, 로그인·로그아웃 시각은
 * data/admin_log.txt 파일에 기록된다(화면에는 보이지 않는다).
 * 왼쪽 메뉴의 [관리자]를 누르면 main.c 의 app_go_to_page() 가
 * 로그인 여부(g_is_admin)를 보고 둘 중 하나로 보낸다.
 * 실제 규칙 검사와 데이터 변경은 admin.c 가 하고, 이 파일은 입력을 읽고 결과를 보여준다.
 */
#include "library.h"
#include "ui.h"
#include <stdio.h>   /* swprintf */
#include <time.h>    /* time */

#define MAX_SLOT_ROWS (SEAT_COUNT * MAX_TIME_SLOTS)   /* 시간대 예약 표의 최대 줄 수 */

#define LEFT_WIDTH   390                 /* 왼쪽 입력 영역 너비 */
#define RIGHT_X      (CONTENT_X + 420)   /* 오른쪽 표가 시작하는 x 좌표 */
#define RIGHT_WIDTH  (CONTENT_WIDTH - 420)

/* 로그인 화면 */
static HWND g_login_edit;
static HWND g_login_button;
static HWND g_login_result;
static LoginGuard g_login_guard = { 0, 0 };   /* 로그인 실패 횟수와 잠금 상태 */

/* 관리자 화면 */
static HWND g_book_id_edit;
static HWND g_title_edit;
static HWND g_author_edit;
static HWND g_student_edit;
static HWND g_seat_edit;
static HWND g_password_edit;
static HWND g_result_label;
static HWND g_user_table;
static HWND g_summary_label;
static HWND g_slot_table;
static int g_slot_seat[MAX_SLOT_ROWS];    /* 시간대 예약 표의 n번째 줄이 몇 번 좌석인지 */
static int g_slot_start[MAX_SLOT_ROWS];   /* 그 줄의 시작 시각 */
static int g_slot_student[MAX_SLOT_ROWS]; /* 그 줄의 예약 학번 */
static int g_slot_row_count = 0;

/* ===================== 로그인, 로그아웃 ===================== */

/* 로그인이 막힌 동안 "N초 뒤에 다시 시도할 수 있습니다"를 보여준다 */
static void show_lock_message(int wait_seconds)
{
    wchar_t text[80];

    swprintf(text, 80, L"%d번 이상 틀렸습니다. %d초 뒤에 다시 시도할 수 있습니다",
             LOGIN_MAX_FAILS, wait_seconds);
    ui_set_label(g_login_result, text, COLOR_RED);
}

/* 로그인을 잠근다: 입력칸과 버튼을 못 쓰게 하고, 1초마다 남은 시간을 세는 타이머를 켠다 */
static void start_lock(int wait_seconds)
{
    EnableWindow(g_login_edit, FALSE);
    EnableWindow(g_login_button, FALSE);
    show_lock_message(wait_seconds);
    SetTimer(ui_main_window(), TIMER_LOGIN_LOCK, 1000, NULL);   /* 1000밀리초 = 1초 */
}

/* 타이머가 1초마다 부른다 (main.c 의 WM_TIMER): 남은 시간을 고쳐 쓰고, 끝났으면 잠금을 푼다 */
void admin_page_timer(void)
{
    int wait_seconds = login_wait_seconds(&g_login_guard, (long long)time(NULL));   /* admin.c */

    if (wait_seconds > 0) {
        show_lock_message(wait_seconds);
        return;
    }
    KillTimer(ui_main_window(), TIMER_LOGIN_LOCK);
    EnableWindow(g_login_edit, TRUE);
    EnableWindow(g_login_button, TRUE);
    ui_set_label(g_login_result, L"다시 로그인할 수 있습니다", COLOR_GRAY);
    if (ui_current_page() == PAGE_LOGIN) {
        SetFocus(g_login_edit);
    }
}

/* [로그인] 버튼 (또는 Enter 키) */
static void do_login(void)
{
    char password[PASSWORD_LEN];
    wchar_t text[80];
    int wait_seconds = 0;
    Result result;

    ui_get_text(g_login_edit, password, PASSWORD_LEN);
    SetWindowTextW(g_login_edit, L"");   /* 입력한 비밀번호는 바로 지운다 */

    result = login_admin(&g_data, &g_login_guard, password, (long long)time(NULL),
                         &wait_seconds);   /* admin.c */

    if (result == RESULT_OK) {
        student_logout();                /* 학생이 로그인해 있었다면 로그아웃시킨다 (page_student.c) */
        g_is_admin = 1;
        append_admin_log("로그인 성공");   /* file.c : data/admin_log.txt 에 시각과 함께 기록 */
        ui_set_label(g_login_result, L"", COLOR_TEXT);
        ui_set_label(g_result_label, L"관리자 모드로 로그인했습니다", COLOR_GREEN);
        ui_refresh_sidebar();            /* 왼쪽 메뉴의 "이용자 모드" → "관리자 모드" */
        app_go_to_page(PAGE_ADMIN);
    } else if (result == ERR_LOGIN_LOCKED) {
        append_admin_log("로그인 실패 - 여러 번 틀려서 잠금");
        start_lock(wait_seconds);
    } else {
        append_admin_log("로그인 실패");
        swprintf(text, 80, L"비밀번호가 틀렸습니다 (%d번 중 %d번째)", LOGIN_MAX_FAILS,
                 g_login_guard.fail_count);
        ui_set_label(g_login_result, text, COLOR_RED);
    }
}

/*
 * 관리자를 로그아웃시킨다. (화면 이동은 하지 않는다. 부른 쪽에서 app_go_to_page 를 부른다)
 * [로그아웃] 버튼과 학생 로그인(page_student.c)에서 쓴다.
 */
void admin_logout(void)
{
    if (!g_is_admin) {
        return;
    }
    g_is_admin = 0;
    append_admin_log("로그아웃");   /* file.c */

    /* 다음 사람에게 보이지 않도록 입력했던 내용을 모두 지운다 */
    SetWindowTextW(g_book_id_edit, L"");
    SetWindowTextW(g_title_edit, L"");
    SetWindowTextW(g_author_edit, L"");
    SetWindowTextW(g_student_edit, L"");
    SetWindowTextW(g_seat_edit, L"");
    SetWindowTextW(g_password_edit, L"");
    ui_set_label(g_result_label, L"", COLOR_TEXT);
    loan_page_reset();   /* page_loan.c */
    seat_page_reset();   /* page_seat.c */
    ui_refresh_sidebar();
}

/* ===================== 관리자 기능 버튼 ===================== */

/* 결과를 라벨에 보여주고, 성공했으면 홈 화면 처리 내역에도 남긴 뒤 화면을 새로 고친다 */
static void finish(Result result, const wchar_t *success_text)
{
    ui_show_result(g_result_label, result, success_text);
    if (result == RESULT_OK) {
        home_add_log(success_text);
    }
    admin_page_refresh();
}

/* [지금 저장] : 종료하지 않고 파일에 저장한다 */
static void do_save(void)
{
    Result result = save_all(&g_data);   /* file.c */

    ui_show_result(g_result_label, result, L"data 폴더에 저장했습니다");
}

/* [도서 추가] */
static void do_add_book(void)
{
    char title[TITLE_LEN];
    char author[AUTHOR_LEN];
    int book_id = ui_get_number(g_book_id_edit);
    wchar_t text[100];
    Result result;

    ui_get_text(g_title_edit, title, TITLE_LEN);      /* 입력칸 → UTF-8 문자열 */
    ui_get_text(g_author_edit, author, AUTHOR_LEN);
    result = admin_add_book(&g_data, book_id, title, author);   /* admin.c */

    swprintf(text, 100, L"[관리자] 도서 추가: %d번", book_id);
    finish(result, text);
    if (result == RESULT_OK) {           /* 다음 도서를 바로 입력할 수 있게 칸을 비운다 */
        SetWindowTextW(g_book_id_edit, L"");
        SetWindowTextW(g_title_edit, L"");
        SetWindowTextW(g_author_edit, L"");
    }
}

/* [도서 삭제] : 도서 번호만 사용한다 */
static void do_remove_book(void)
{
    int book_id = ui_get_number(g_book_id_edit);
    wchar_t text[100];

    swprintf(text, 100, L"[관리자] 도서 삭제: %d번", book_id);
    finish(admin_remove_book(&g_data, book_id), text);
}

/* [관리자 반납] : 학번 없이 도서 번호만으로 반납 처리한다 */
static void do_return_book(void)
{
    int book_id = ui_get_number(g_book_id_edit);
    wchar_t text[100];

    swprintf(text, 100, L"[관리자] 반납 처리: %d번 도서", book_id);
    finish(admin_return_book(&g_data, book_id), text);
}

/*
 * 방금 발급한 임시 비밀번호를 창으로 한 번만 보여준다.
 * 프로그램은 해시만 기억하므로 이 창을 닫으면 다시 볼 수 없다. (처리 내역에도 남기지 않는다)
 */
static void show_temp_password(int student_id, const char *temp_password)
{
    wchar_t wide_password[TEMP_PASSWORD_LEN];
    wchar_t text[200];

    ui_utf8_to_wide(temp_password, wide_password, TEMP_PASSWORD_LEN);
    swprintf(text, 200,
             L"학번: %d\n임시 비밀번호: %ls\n\n학생에게 전달하세요. 이 창을 닫으면 다시 볼 수 없습니다.\n"
             L"학생은 이 비밀번호로 로그인한 뒤 새 비밀번호로 바꿔야 합니다.",
             student_id, wide_password);
    MessageBoxW(ui_main_window(), text, L"임시 비밀번호 발급", MB_OK | MB_ICONINFORMATION);
}

/* [학번 등록] : 학생 계정을 만들고 임시 비밀번호를 발급한다 */
static void do_add_user(void)
{
    int student_id = ui_get_number(g_student_edit);
    char temp_password[TEMP_PASSWORD_LEN];
    wchar_t text[100];
    Result result = admin_add_user(&g_data, student_id, temp_password);   /* admin.c */

    swprintf(text, 100, L"[관리자] 학번 등록: %d", student_id);
    finish(result, text);
    if (result == RESULT_OK) {
        SetWindowTextW(g_student_edit, L"");
        show_temp_password(student_id, temp_password);
    }
}

/* [학번 삭제] */
static void do_remove_user(void)
{
    int student_id = ui_get_number(g_student_edit);
    wchar_t text[100];

    swprintf(text, 100, L"[관리자] 학번 삭제: %d", student_id);
    finish(admin_remove_user(&g_data, student_id), text);
}

/* [비번 초기화] : 비밀번호를 잊은 학생에게 임시 비밀번호를 다시 발급한다 */
static void do_reset_password(void)
{
    int student_id = ui_get_number(g_student_edit);
    char temp_password[TEMP_PASSWORD_LEN];
    wchar_t text[100];
    Result result = admin_reset_password(&g_data, student_id, temp_password);   /* admin.c */

    swprintf(text, 100, L"[관리자] 비밀번호 초기화: %d", student_id);
    finish(result, text);
    if (result == RESULT_OK) {
        SetWindowTextW(g_student_edit, L"");
        show_temp_password(student_id, temp_password);
    }
}

/* [좌석 비우기] */
static void do_clear_seat(void)
{
    int seat_number = ui_get_number(g_seat_edit);
    wchar_t text[100];

    swprintf(text, 100, L"[관리자] 좌석 비우기: %02d번", seat_number);
    finish(admin_clear_seat(&g_data, seat_number), text);
}

/* [하루 마감] : 되돌릴 수 없으므로 한 번 더 물어본 뒤 모든 좌석 예약을 비운다 */
static void do_close_day(void)
{
    int answer = MessageBoxW(ui_main_window(),
                             L"모든 좌석 예약과 시간대 예약을 비웁니다.\n(도서 대출은 그대로 둡니다) 계속할까요?",
                             L"하루 마감", MB_YESNO | MB_ICONQUESTION);

    if (answer == IDYES) {
        admin_close_day(&g_data);
        finish(RESULT_OK, L"[관리자] 하루 마감: 모든 좌석 예약을 비웠습니다");
    }
}

/* [선택한 예약 취소] : 시간대 예약 현황 표에서 고른 예약을, 그 예약의 학번으로 취소한다 */
static void do_cancel_slot(void)
{
    int row = ui_table_selected_row(g_slot_table);
    wchar_t text[100];
    Result result;

    if (row < 0 || row >= g_slot_row_count) {
        ui_set_label(g_result_label, L"먼저 시간대 예약 현황 표에서 취소할 예약을 고르세요", COLOR_RED);
        return;
    }
    result = cancel_time_slot(&g_data.seats[g_slot_seat[row] - 1], g_slot_start[row],
                              g_slot_student[row]);   /* seat.c */

    swprintf(text, 100, L"[관리자] 시간대 예약 취소: %02d번 좌석 %d시", g_slot_seat[row],
             g_slot_start[row]);
    finish(result, text);
}

/* [변경] : 관리자 비밀번호를 바꾼다 */
static void do_change_password(void)
{
    char password[PASSWORD_LEN];
    Result result;

    ui_get_text(g_password_edit, password, PASSWORD_LEN);
    SetWindowTextW(g_password_edit, L"");
    result = change_admin_password(&g_data, password);   /* admin.c */

    /* 비밀번호는 [지금 저장]이나 [저장 후 종료]를 해야 admin.txt 에 남는다.
     * 비밀번호를 바꾼 사실은 홈 화면의 처리 내역에 남기지 않는다 */
    ui_show_result(g_result_label, result,
                   L"비밀번호를 바꿨습니다 (저장해야 다음 실행에도 적용돼요)");
}

/* ===================== 화면 만들기, 새로 고침 ===================== */

/* 로그인 화면의 컨트롤을 만든다 */
static void create_login_page(void)
{
    ui_label(PAGE_LOGIN, L"관리자 로그인", CONTENT_X, 28, 400, 40, FONT_TITLE, COLOR_TEXT);
    ui_label(PAGE_LOGIN, L"도서·학번·좌석 관리와 저장 후 종료는 관리자(조교·근로학생)만 할 수 있어요",
             CONTENT_X, 76, CONTENT_WIDTH, 24, FONT_NORMAL, COLOR_GRAY);

    ui_label(PAGE_LOGIN, L"관리자 비밀번호", CONTENT_X, 126, 300, 22, FONT_BOLD, COLOR_GRAY);
    g_login_edit = ui_edit(PAGE_LOGIN, ID_LOGIN_EDIT, CONTENT_X, 150, 300, 32, EDIT_PASSWORD);
    g_login_button = ui_button(PAGE_LOGIN, ID_LOGIN_BUTTON, L"로그인", CONTENT_X + 315, 150, 130,
                               32);
    g_login_result = ui_label(PAGE_LOGIN, L"", CONTENT_X, 196, CONTENT_WIDTH, 24, FONT_BOLD,
                              COLOR_TEXT);

    ui_label(PAGE_LOGIN, L"학생은 왼쪽 메뉴의 [학생 로그인]에서 학번과 비밀번호로 로그인하세요",
             CONTENT_X, 250, CONTENT_WIDTH, 24, FONT_NORMAL, COLOR_GRAY);
    ui_label(PAGE_LOGIN, L"비밀번호를 5번 연속으로 틀리면 10초 동안 로그인할 수 없고, 로그인 기록은 파일에 남아요",
             CONTENT_X, 278, CONTENT_WIDTH, 24, FONT_NORMAL, COLOR_GRAY);
}

/* 관리자 화면의 컨트롤을 만든다 */
static void create_admin_page(void)
{
    ui_label(PAGE_ADMIN, L"관리자", CONTENT_X, 28, 300, 40, FONT_TITLE, COLOR_TEXT);
    ui_button(PAGE_ADMIN, ID_ADMIN_SAVE, L"지금 저장", CONTENT_X + 516, 32, 120, 34);
    ui_button(PAGE_ADMIN, ID_ADMIN_LOGOUT, L"로그아웃", CONTENT_X + 650, 32, 120, 34);
    ui_label(PAGE_ADMIN, L"바꾼 내용은 [지금 저장]이나 [저장 후 종료]를 눌러야 파일에 남아요",
             CONTENT_X, 76, CONTENT_WIDTH, 24, FONT_NORMAL, COLOR_GRAY);

    /* 도서 관리 */
    ui_label(PAGE_ADMIN, L"도서 관리 (삭제와 반납은 번호만 입력)", CONTENT_X, 112, LEFT_WIDTH, 22,
             FONT_BOLD, COLOR_TEXT);
    ui_label(PAGE_ADMIN, L"번호", CONTENT_X, 138, 80, 20, FONT_NORMAL, COLOR_GRAY);
    ui_label(PAGE_ADMIN, L"제목", CONTENT_X + 90, 138, 180, 20, FONT_NORMAL, COLOR_GRAY);
    ui_label(PAGE_ADMIN, L"저자", CONTENT_X + 280, 138, 110, 20, FONT_NORMAL, COLOR_GRAY);
    g_book_id_edit = ui_edit(PAGE_ADMIN, ID_ADMIN_BOOK_ID, CONTENT_X, 160, 80, 30, EDIT_NUMBER);
    g_title_edit = ui_edit(PAGE_ADMIN, ID_ADMIN_TITLE, CONTENT_X + 90, 160, 180, 30, EDIT_TEXT);
    g_author_edit = ui_edit(PAGE_ADMIN, ID_ADMIN_AUTHOR, CONTENT_X + 280, 160, 110, 30, EDIT_TEXT);
    /* 한글은 한 글자가 3바이트라서, 구조체의 char 배열 크기에 맞게 글자 수를 제한한다 */
    ui_edit_limit(g_title_edit, (TITLE_LEN - 1) / 3);     /* 79 / 3 = 26글자 */
    ui_edit_limit(g_author_edit, (AUTHOR_LEN - 1) / 3);   /* 39 / 3 = 13글자 */
    ui_button(PAGE_ADMIN, ID_ADMIN_BOOK_ADD, L"도서 추가", CONTENT_X, 200, 122, 34);
    ui_button(PAGE_ADMIN, ID_ADMIN_BOOK_REMOVE, L"도서 삭제", CONTENT_X + 134, 200, 122, 34);
    ui_button(PAGE_ADMIN, ID_ADMIN_BOOK_RETURN, L"관리자 반납", CONTENT_X + 268, 200, 122, 34);

    /* 학번 관리 */
    ui_label(PAGE_ADMIN, L"학생 계정 (학번 8자리, 등록하면 임시 비밀번호 발급)", CONTENT_X, 252,
             LEFT_WIDTH, 22, FONT_BOLD, COLOR_TEXT);
    g_student_edit = ui_edit(PAGE_ADMIN, ID_ADMIN_STUDENT, CONTENT_X, 278, 100, 32, EDIT_NUMBER);
    ui_button(PAGE_ADMIN, ID_ADMIN_USER_ADD, L"학번 등록", CONTENT_X + 108, 278, 90, 32);
    ui_button(PAGE_ADMIN, ID_ADMIN_USER_REMOVE, L"학번 삭제", CONTENT_X + 204, 278, 90, 32);
    ui_button(PAGE_ADMIN, ID_ADMIN_USER_RESET, L"비번 초기화", CONTENT_X + 300, 278, 90, 32);

    /* 좌석 관리 */
    ui_label(PAGE_ADMIN, L"좌석 관리 (좌석 번호 1~12)", CONTENT_X, 328, LEFT_WIDTH, 22, FONT_BOLD,
             COLOR_TEXT);
    g_seat_edit = ui_edit(PAGE_ADMIN, ID_ADMIN_SEAT, CONTENT_X, 354, 140, 32, EDIT_NUMBER);
    ui_button(PAGE_ADMIN, ID_ADMIN_SEAT_CLEAR, L"좌석 비우기", CONTENT_X + 154, 354, 112, 32);
    ui_button(PAGE_ADMIN, ID_ADMIN_CLOSE_DAY, L"하루 마감", CONTENT_X + 278, 354, 112, 32);

    /* 비밀번호 변경 */
    ui_label(PAGE_ADMIN, L"비밀번호 변경 (영문·숫자 4~20글자)", CONTENT_X, 404, LEFT_WIDTH, 22,
             FONT_BOLD, COLOR_TEXT);
    g_password_edit = ui_edit(PAGE_ADMIN, ID_ADMIN_PASSWORD, CONTENT_X, 430, 266, 32,
                              EDIT_PASSWORD);
    ui_button(PAGE_ADMIN, ID_ADMIN_PW_CHANGE, L"변경", CONTENT_X + 278, 430, 112, 32);

    g_result_label = ui_label(PAGE_ADMIN, L"", CONTENT_X, 482, LEFT_WIDTH, 70, FONT_BOLD,
                              COLOR_TEXT);

    /* 오른쪽 위: 등록 학번 현황 표 */
    ui_label(PAGE_ADMIN, L"등록 학번 현황", RIGHT_X, 112, RIGHT_WIDTH, 22, FONT_BOLD, COLOR_TEXT);
    g_user_table = ui_table(PAGE_ADMIN, ID_ADMIN_USER_TABLE, RIGHT_X, 138, RIGHT_WIDTH, 176);
    ui_table_add_column(g_user_table, 0, L"학번", 100);
    ui_table_add_column(g_user_table, 1, L"대출", 60);
    ui_table_add_column(g_user_table, 2, L"좌석", 60);
    ui_table_add_column(g_user_table, 3, L"비밀번호", 90);

    /* 오른쪽 아래: 시간대 예약 현황 표와 취소 버튼 */
    ui_label(PAGE_ADMIN, L"시간대 예약 현황 (줄을 고르고 취소)", RIGHT_X, 328, RIGHT_WIDTH, 22,
             FONT_BOLD, COLOR_TEXT);
    g_slot_table = ui_table(PAGE_ADMIN, ID_ADMIN_SLOT_TABLE, RIGHT_X, 354, RIGHT_WIDTH, 160);
    ui_table_add_column(g_slot_table, 0, L"좌석", 60);
    ui_table_add_column(g_slot_table, 1, L"시간", 110);
    ui_table_add_column(g_slot_table, 2, L"학번", 120);
    ui_button(PAGE_ADMIN, ID_ADMIN_SLOT_CANCEL, L"선택한 예약 취소", RIGHT_X, 522, 170, 32);

    g_summary_label = ui_label(PAGE_ADMIN, L"", RIGHT_X, 566, RIGHT_WIDTH, 48, FONT_NORMAL,
                               COLOR_GRAY);
}

/* 로그인 화면과 관리자 화면의 컨트롤을 만든다 (프로그램 시작 시 한 번) */
void admin_page_create(void)
{
    create_login_page();
    create_admin_page();
}

/* 시간대 예약 현황 표를 다시 채운다: 모든 좌석의 시간대 예약을 좌석 순서대로 */
static void fill_slot_table(void)
{
    int i, j;

    ui_table_clear(g_slot_table);
    g_slot_row_count = 0;
    for (i = 0; i < SEAT_COUNT; i++) {
        const Seat *seat = &g_data.seats[i];

        for (j = 0; j < seat->slot_count && g_slot_row_count < MAX_SLOT_ROWS; j++) {
            wchar_t seat_text[12];
            wchar_t time_text[24];
            wchar_t student_text[16];
            const wchar_t *cells[3];

            swprintf(seat_text, 12, L"%02d번", seat->number);
            swprintf(time_text, 24, L"%d시 ~ %d시", seat->slots[j].start_hour,
                     seat->slots[j].end_hour);
            swprintf(student_text, 16, L"%d", seat->slots[j].student_id);
            cells[0] = seat_text;
            cells[1] = time_text;
            cells[2] = student_text;
            ui_table_add_row(g_slot_table, cells, 3);

            /* 나중에 [선택한 예약 취소]에서 쓰려고, 이 줄이 어떤 예약인지 기억해 둔다 */
            g_slot_seat[g_slot_row_count] = seat->number;
            g_slot_start[g_slot_row_count] = seat->slots[j].start_hour;
            g_slot_student[g_slot_row_count] = seat->slots[j].student_id;
            g_slot_row_count++;
        }
    }
}

/* 등록 학번 현황 표, 시간대 예약 현황 표, 요약 글을 최신 데이터로 다시 채운다 */
void admin_page_refresh(void)
{
    wchar_t summary[120];
    int i;

    fill_slot_table();

    ui_table_clear(g_user_table);
    for (i = 0; i < g_data.user_count; i++) {
        int student_id = g_data.users[i].id;
        int seat_number = find_user_seat(&g_data, student_id);   /* admin.c */
        wchar_t id_text[12];
        wchar_t book_text[12];
        wchar_t seat_text[12];
        const wchar_t *cells[4];

        swprintf(id_text, 12, L"%d", student_id);
        swprintf(book_text, 12, L"%d권",
                 count_student_loans(g_data.books, g_data.book_count, student_id));   /* loan.c */
        if (seat_number == 0) {
            swprintf(seat_text, 12, L"-");
        } else {
            swprintf(seat_text, 12, L"%02d번", seat_number);
        }
        cells[0] = id_text;
        cells[1] = book_text;
        cells[2] = seat_text;
        if (g_data.users[i].must_change) {
            cells[3] = L"임시";      /* 아직 임시 비밀번호를 쓰는 학생 */
        } else {
            cells[3] = L"변경함";    /* 학생이 직접 정한 비밀번호 (관리자도 알 수 없다) */
        }
        ui_table_add_row(g_user_table, cells, 4);
    }

    /* 도서 배열 크기를 보여 주면, 도서를 추가할 때 realloc 으로 늘어나는 것을 확인할 수 있다 */
    swprintf(summary, 120, L"등록 학번 %d명 · 도서 %d권\n도서 배열 크기 %d칸 (꽉 차면 2배로 늘어남)",
             g_data.user_count, g_data.book_count, g_data.book_capacity);
    ui_set_label(g_summary_label, summary, COLOR_GRAY);
}

/* 로그인 화면으로 왔을 때: 비밀번호 입력칸을 비우고 커서를 둔다 */
void admin_page_focus_login(void)
{
    SetWindowTextW(g_login_edit, L"");
    SetFocus(g_login_edit);
}

/* 버튼 클릭 처리 */
void admin_page_command(int id, int code)
{
    if (code != BN_CLICKED) {
        return;
    }
    if (id == ID_LOGIN_BUTTON) {
        if (!g_is_admin) {   /* 이미 로그인했으면 다시 검사하지 않는다 */
            do_login();
        }
        return;
    }
    if (!g_is_admin) {   /* 아래 기능은 모두 관리자 모드에서만 동작한다 */
        return;
    }

    if (id == ID_ADMIN_SAVE) {
        do_save();
    } else if (id == ID_ADMIN_LOGOUT) {
        admin_logout();
        app_go_to_page(PAGE_HOME);
    } else if (id == ID_ADMIN_BOOK_ADD) {
        do_add_book();
    } else if (id == ID_ADMIN_BOOK_REMOVE) {
        do_remove_book();
    } else if (id == ID_ADMIN_BOOK_RETURN) {
        do_return_book();
    } else if (id == ID_ADMIN_USER_ADD) {
        do_add_user();
    } else if (id == ID_ADMIN_USER_REMOVE) {
        do_remove_user();
    } else if (id == ID_ADMIN_USER_RESET) {
        do_reset_password();
    } else if (id == ID_ADMIN_SEAT_CLEAR) {
        do_clear_seat();
    } else if (id == ID_ADMIN_CLOSE_DAY) {
        do_close_day();
    } else if (id == ID_ADMIN_SLOT_CANCEL) {
        do_cancel_slot();
    } else if (id == ID_ADMIN_PW_CHANGE) {
        do_change_password();
    }
}
