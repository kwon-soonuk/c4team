/*
 * page_student.c  (담당: 최민호)  F8 학생 로그인 화면 + 내 정보 화면
 * ------------------------------------------------------------------
 * 화면이 두 개다.
 *   PAGE_STUDENT_LOGIN : 학번·비밀번호 입력칸과 [로그인] 버튼
 *   PAGE_STUDENT       : 내 정보 (비밀번호 변경, 로그아웃)
 * 왼쪽 메뉴의 [학생 로그인](로그인하면 [내 정보])을 누르면 main.c 의 app_go_to_page() 가
 * 로그인 여부(g_student_id)를 보고 둘 중 하나로 보낸다.
 *
 * 임시 비밀번호로 로그인한 학생은 비밀번호를 바꾸기 전까지 내 정보 화면만 볼 수 있다.
 * 규칙 검사(비밀번호 확인, 로그인 시도 제한, 비밀번호 형식)는 user.c 가 한다.
 */
#include "library.h"
#include "ui.h"
#include <stdio.h>   /* swprintf */
#include <string.h>  /* strcmp */
#include <time.h>    /* time */

/* 로그인 화면 */
static HWND g_id_edit;
static HWND g_password_edit;
static HWND g_login_result;
static LoginGuard g_login_guard = { 0, 0 };   /* 학생 로그인 실패 횟수와 잠금 상태 */

/* 내 정보 화면 */
static HWND g_who_label;
static HWND g_notice_label;
static HWND g_new_edit;
static HWND g_new_edit2;
static HWND g_result_label;

/* ===================== 로그인, 로그아웃 ===================== */

/* [로그인] 버튼 (또는 Enter 키) */
static void do_login(void)
{
    char password[PASSWORD_LEN];
    wchar_t text[100];
    int student_id = ui_get_number(g_id_edit);
    int wait_seconds = 0;
    Result result;

    ui_get_text(g_password_edit, password, PASSWORD_LEN);
    SetWindowTextW(g_password_edit, L"");   /* 입력한 비밀번호는 바로 지운다 */

    result = login_student(&g_data, &g_login_guard, student_id, password,
                           (long long)time(NULL), &wait_seconds);   /* user.c */

    if (result == RESULT_OK) {
        admin_logout();                  /* 관리자가 로그인해 있었다면 로그아웃시킨다 */
        g_student_id = student_id;
        SetWindowTextW(g_id_edit, L"");
        ui_set_label(g_login_result, L"", COLOR_TEXT);
        ui_set_label(g_result_label, L"", COLOR_TEXT);
        home_set_notice(L"");
        ui_refresh_sidebar();            /* 왼쪽 메뉴: "학생 로그인" → "내 정보", 학번 표시 */
        /* 임시 비밀번호라면 app_go_to_page 가 홈 대신 내 정보(비밀번호 변경) 화면으로 보낸다 */
        app_go_to_page(PAGE_HOME);
    } else if (result == ERR_LOGIN_LOCKED) {
        swprintf(text, 100, L"%d번 이상 틀렸습니다. %d초 뒤에 다시 시도하세요", LOGIN_MAX_FAILS,
                 wait_seconds);
        ui_set_label(g_login_result, text, COLOR_RED);
    } else {
        /* 학번이 없는 경우와 비밀번호가 틀린 경우를 구분해서 알려 주지 않는다 */
        ui_set_label(g_login_result, L"학번 또는 비밀번호가 틀렸습니다", COLOR_RED);
    }
}

/*
 * 학생을 로그아웃시킨다. (화면 이동은 하지 않는다. 부른 쪽에서 app_go_to_page 를 부른다)
 * [로그아웃] 버튼, 자동 로그아웃(main.c), 관리자 로그인(page_admin.c)에서 쓴다.
 */
void student_logout(void)
{
    if (g_student_id == 0) {
        return;
    }
    g_student_id = 0;

    /* 다음 사람에게 보이지 않도록 입력했던 내용과 결과 안내를 모두 지운다 */
    SetWindowTextW(g_new_edit, L"");
    SetWindowTextW(g_new_edit2, L"");
    ui_set_label(g_result_label, L"", COLOR_TEXT);
    loan_page_reset();   /* page_loan.c */
    seat_page_reset();   /* page_seat.c */
    ui_refresh_sidebar();
}

/* [비밀번호 변경] 버튼 (또는 Enter 키) */
static void do_change_password(void)
{
    char password[PASSWORD_LEN];
    char password2[PASSWORD_LEN];
    Result result;

    ui_get_text(g_new_edit, password, PASSWORD_LEN);
    ui_get_text(g_new_edit2, password2, PASSWORD_LEN);
    SetWindowTextW(g_new_edit, L"");
    SetWindowTextW(g_new_edit2, L"");

    if (strcmp(password, password2) != 0) {   /* 두 칸에 같은 글자를 넣었는지 */
        ui_set_label(g_result_label, L"두 칸에 입력한 비밀번호가 서로 다릅니다", COLOR_RED);
        return;
    }
    result = change_student_password(&g_data, g_student_id, password);   /* user.c */
    if (result == RESULT_OK) {
        ui_set_label(g_result_label, L"비밀번호를 바꿨습니다", COLOR_GREEN);
        student_page_refresh();   /* "임시 비밀번호입니다" 안내를 지운다 */
    } else {
        ui_set_label(g_result_label, ui_result_text(result), COLOR_RED);
    }
}

/* ===================== 화면 만들기, 새로 고침 ===================== */

static void create_login_page(void)
{
    int page = PAGE_STUDENT_LOGIN;

    ui_label(page, L"학생 로그인", CONTENT_X, 28, 400, 40, FONT_TITLE, COLOR_TEXT);
    ui_label(page, L"대출 · 반납 · 좌석 예약은 로그인한 뒤에 이용할 수 있어요",
             CONTENT_X, 76, CONTENT_WIDTH, 24, FONT_NORMAL, COLOR_GRAY);

    ui_label(page, L"학번", CONTENT_X, 126, 300, 22, FONT_BOLD, COLOR_GRAY);
    g_id_edit = ui_edit(page, ID_STUDENT_ID_EDIT, CONTENT_X, 150, 300, 32, EDIT_NUMBER);
    ui_label(page, L"비밀번호", CONTENT_X, 198, 300, 22, FONT_BOLD, COLOR_GRAY);
    g_password_edit = ui_edit(page, ID_STUDENT_PW_EDIT, CONTENT_X, 222, 300, 32, EDIT_PASSWORD);
    ui_button(page, ID_STUDENT_LOGIN, L"로그인", CONTENT_X + 315, 222, 130, 32);
    g_login_result = ui_label(page, L"", CONTENT_X, 268, CONTENT_WIDTH, 24, FONT_BOLD, COLOR_TEXT);

    ui_label(page,
             L"처음 이용하는 학생은 조교(관리자)에게 학번 등록을 요청하고 임시 비밀번호를 받으세요.\n"
             L"임시 비밀번호로 로그인하면 새 비밀번호로 바꾼 뒤에 이용할 수 있어요.\n"
             L"비밀번호를 잊었으면 조교에게 비밀번호 초기화를 요청하세요.",
             CONTENT_X, 320, CONTENT_WIDTH, 80, FONT_NORMAL, COLOR_GRAY);
}

static void create_my_page(void)
{
    int page = PAGE_STUDENT;

    ui_label(page, L"내 정보", CONTENT_X, 28, 300, 40, FONT_TITLE, COLOR_TEXT);
    ui_button(page, ID_STUDENT_LOGOUT, L"로그아웃", CONTENT_X + 650, 32, 120, 34);
    g_who_label = ui_label(page, L"", CONTENT_X, 76, CONTENT_WIDTH, 24, FONT_NORMAL, COLOR_GRAY);
    g_notice_label = ui_label(page, L"", CONTENT_X, 116, CONTENT_WIDTH, 48, FONT_BOLD, COLOR_RED);

    ui_label(page, L"새 비밀번호 (영문·숫자 4~20글자)", CONTENT_X, 180, 400, 22, FONT_BOLD,
             COLOR_GRAY);
    g_new_edit = ui_edit(page, ID_STUDENT_NEW_PW, CONTENT_X, 204, 300, 32, EDIT_PASSWORD);
    ui_label(page, L"새 비밀번호 한 번 더", CONTENT_X, 252, 400, 22, FONT_BOLD, COLOR_GRAY);
    g_new_edit2 = ui_edit(page, ID_STUDENT_NEW_PW2, CONTENT_X, 276, 300, 32, EDIT_PASSWORD);
    ui_button(page, ID_STUDENT_PW_CHANGE, L"비밀번호 변경", CONTENT_X + 315, 276, 150, 32);
    g_result_label = ui_label(page, L"", CONTENT_X, 322, CONTENT_WIDTH, 24, FONT_BOLD, COLOR_TEXT);

    ui_label(page,
             L"내가 빌린 책과 내 좌석은 홈 화면에서 볼 수 있어요.\n"
             L"자리를 뜰 때는 꼭 [로그아웃]을 누르세요. 1분 동안 사용하지 않으면 자동으로 로그아웃됩니다.",
             CONTENT_X, 372, CONTENT_WIDTH, 56, FONT_NORMAL, COLOR_GRAY);
}

/* 학생 로그인 화면과 내 정보 화면의 컨트롤을 만든다 (프로그램 시작 시 한 번) */
void student_page_create(void)
{
    create_login_page();
    create_my_page();
}

/* 내 정보 화면의 글을 지금 로그인한 학생에 맞게 바꾼다 */
void student_page_refresh(void)
{
    wchar_t text[80];

    if (g_student_id == 0) {
        return;
    }
    swprintf(text, 80, L"학번 %d(으)로 로그인했습니다", g_student_id);
    ui_set_label(g_who_label, text, COLOR_GRAY);

    if (must_change_password(&g_data, g_student_id)) {   /* user.c */
        ui_set_label(g_notice_label,
                     L"임시 비밀번호로 로그인했습니다.\n새 비밀번호로 바꿔야 다른 기능을 이용할 수 있습니다.",
                     COLOR_RED);
        if (ui_current_page() == PAGE_STUDENT) {
            SetFocus(g_new_edit);
        }
    } else {
        ui_set_label(g_notice_label, L"", COLOR_RED);
    }
}

/* 로그인 화면으로 왔을 때: 비밀번호 칸을 비우고 학번 칸에 커서를 둔다 */
void student_page_focus_login(void)
{
    SetWindowTextW(g_password_edit, L"");
    SetFocus(g_id_edit);
}

/* 로그인하지 않고 대출·예약 버튼을 눌렀을 때: 안내와 함께 로그인 화면으로 보낸다 (main.c 에서 호출) */
void student_page_ask_login(void)
{
    ui_set_label(g_login_result, L"로그인한 뒤에 이용할 수 있습니다", COLOR_BLUE);
    app_go_to_page(PAGE_STUDENT_LOGIN);
}

/* 버튼 클릭 처리 */
void student_page_command(int id, int code)
{
    if (code != BN_CLICKED) {
        return;
    }
    if (id == ID_STUDENT_LOGIN) {
        if (g_student_id == 0) {   /* 이미 로그인했으면 다시 검사하지 않는다 */
            do_login();
        }
        return;
    }
    if (g_student_id == 0) {       /* 아래 기능은 로그인한 학생만 쓸 수 있다 */
        return;
    }

    if (id == ID_STUDENT_PW_CHANGE) {
        do_change_password();
    } else if (id == ID_STUDENT_LOGOUT) {
        student_logout();
        home_set_notice(L"로그아웃했습니다");
        app_go_to_page(PAGE_HOME);
    }
}
