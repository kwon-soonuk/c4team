/*
 * ui.h  (담당: 권순욱)
 * ------------------------------------------------------------------
 * 화면(Win32) 코드끼리 함께 쓰는 약속
 *   - 창 크기, 화면(페이지) 번호, 컨트롤 ID, 색, 글꼴 종류
 *   - ui.c 의 공통 화면 함수, page_*.c 의 화면별 함수 선언
 *
 * 로직 파일(search.c, loan.c, seat.c, file.c, user.c, admin.c, crypto.c)은 이 파일을 include 하지 않는다.
 * → "계산하는 코드"와 "화면 코드"가 분리되어, 각자 따로 고치고 설명할 수 있다.
 */
#ifndef UI_H
#define UI_H

#include "library.h"
#include <windows.h>

/* ===== 창 크기 (픽셀) =====
 * 노트북 화면 배율(125%, 150%)에서도 작업 표시줄에 가리지 않도록 높이를 640으로 정했다 */
#define WINDOW_WIDTH   1040
#define WINDOW_HEIGHT  640
#define SIDEBAR_WIDTH  210    /* 왼쪽 남색 메뉴 영역 너비 */
#define CONTENT_X      240    /* 오른쪽 내용 영역이 시작하는 x 좌표 */
#define CONTENT_WIDTH  770    /* 내용 영역 너비 (240 ~ 1010) */

/* ===== 화면(페이지) 번호 ===== */
#define PAGE_ALL     (-1)     /* 모든 화면에서 보이는 컨트롤 (왼쪽 메뉴 버튼) */
#define PAGE_HOME    0
#define PAGE_SEARCH  1
#define PAGE_LOAN    2
#define PAGE_SEAT    3
#define PAGE_STUDENT 4        /* 내 정보 화면 (학생이 로그인한 뒤에만 볼 수 있다) */
#define PAGE_ADMIN   5        /* 관리자 화면 (관리자가 로그인한 뒤에만 볼 수 있다) */
#define PAGE_LOGIN   6        /* 관리자 로그인 화면 */
#define PAGE_STUDENT_LOGIN 7  /* 학생 로그인 화면 */

/* ===== 컨트롤 ID =====
 * 버튼을 누르면 WM_COMMAND 메시지에 이 번호가 담겨 온다. → 어떤 버튼인지 구분
 * 화면마다 100 단위로 나눠서 번호가 겹치지 않게 했다.
 * (1과 2는 윈도우가 Enter, Esc 키에 쓰는 번호(IDOK, IDCANCEL)라서 쓰지 않는다) */
#define ID_MENU_HOME          10    /* 10~15 : 메뉴 버튼 (ID - 10 = 화면 번호) */
#define ID_MENU_SEARCH        11
#define ID_MENU_LOAN          12
#define ID_MENU_SEAT          13
#define ID_MENU_STUDENT       14    /* 학생 로그인 / 내 정보 */
#define ID_MENU_ADMIN         15
#define ID_MENU_EXIT          16    /* 저장 후 종료 (관리자만) */

#define ID_HOME_TILE_SEARCH   101   /* 101~103 : 홈 화면 카드 3개 */
#define ID_HOME_TILE_LOAN     102
#define ID_HOME_TILE_SEAT     103
#define ID_HOME_LOG           104
#define ID_HOME_MY_TABLE      105

#define ID_SEARCH_EDIT        201
#define ID_SEARCH_BUTTON      202
#define ID_SEARCH_TABLE       203

#define ID_LOAN_BOOK_EDIT     301
#define ID_LOAN_STUDENT_EDIT  302
#define ID_LOAN_BORROW        303
#define ID_LOAN_RETURN        304
#define ID_LOAN_TABLE         305

#define ID_SEAT_FIRST         401   /* 401~412 : 좌석 버튼 12개 (ID - 400 = 좌석 번호) */
#define ID_SEAT_LAST          412
#define ID_SEAT_STUDENT_EDIT  420
#define ID_SEAT_RESERVE       421
#define ID_SEAT_CANCEL        422
#define ID_SLOT_START         430
#define ID_SLOT_END           431
#define ID_SLOT_RESERVE       432
#define ID_SLOT_TABLE         433

#define ID_LOGIN_EDIT         501   /* 501~502 : 관리자 로그인 화면 */
#define ID_LOGIN_BUTTON       502
#define ID_ADMIN_SAVE         510   /* 510~ : 관리자 화면 */
#define ID_ADMIN_LOGOUT       511
#define ID_ADMIN_BOOK_ID      520
#define ID_ADMIN_TITLE        521
#define ID_ADMIN_AUTHOR       522
#define ID_ADMIN_BOOK_ADD     523
#define ID_ADMIN_BOOK_REMOVE  524
#define ID_ADMIN_BOOK_RETURN  525
#define ID_ADMIN_STUDENT      530
#define ID_ADMIN_USER_ADD     531
#define ID_ADMIN_USER_REMOVE  532
#define ID_ADMIN_USER_RESET   533
#define ID_ADMIN_SEAT         540
#define ID_ADMIN_SEAT_CLEAR   541
#define ID_ADMIN_CLOSE_DAY    542
#define ID_ADMIN_PASSWORD     550
#define ID_ADMIN_PW_CHANGE    551
#define ID_ADMIN_USER_TABLE   560
#define ID_ADMIN_SLOT_TABLE   561
#define ID_ADMIN_SLOT_CANCEL  562

#define ID_STUDENT_ID_EDIT    601   /* 601~603 : 학생 로그인 화면 */
#define ID_STUDENT_PW_EDIT    602
#define ID_STUDENT_LOGIN      603
#define ID_STUDENT_NEW_PW     610   /* 610~ : 내 정보 화면 */
#define ID_STUDENT_NEW_PW2    611
#define ID_STUDENT_PW_CHANGE  612
#define ID_STUDENT_LOGOUT     613

#define TIMER_LOGIN_LOCK      1     /* 관리자 로그인 잠금 시간을 1초마다 세는 타이머 번호 */
#define TIMER_IDLE            2     /* 학생 자동 로그아웃을 1초마다 확인하는 타이머 번호 */

/* ===== 색 (빨강, 초록, 파랑 0~255) ===== */
#define COLOR_NAVY          RGB(22, 44, 92)
#define COLOR_NAVY_LIGHT    RGB(46, 72, 128)
#define COLOR_BLUE          RGB(3, 87, 255)
#define COLOR_LIGHT_BLUE    RGB(232, 240, 255)
#define COLOR_GREEN         RGB(22, 150, 80)
#define COLOR_LIGHT_GREEN   RGB(222, 245, 230)
#define COLOR_ORANGE        RGB(214, 120, 20)
#define COLOR_LIGHT_ORANGE  RGB(255, 237, 212)
#define COLOR_RED           RGB(214, 52, 52)
#define COLOR_WHITE         RGB(255, 255, 255)
#define COLOR_TEXT          RGB(30, 41, 59)
#define COLOR_GRAY          RGB(100, 112, 130)
#define COLOR_SIDEBAR_TEXT  RGB(190, 205, 235)
#define COLOR_ADMIN_MODE    RGB(255, 196, 120)   /* 왼쪽 메뉴의 "관리자 모드" 글자색 */

/* ===== 글꼴 종류 ===== */
#define FONT_NORMAL  0
#define FONT_BOLD    1
#define FONT_TITLE   2

/* ===== 입력칸 종류 (ui_edit 의 kind) ===== */
#define EDIT_TEXT      0   /* 글자 */
#define EDIT_NUMBER    1   /* 숫자만 */
#define EDIT_PASSWORD  2   /* 비밀번호 (입력한 글자가 ● 로 보인다) */

/* 프로그램 전체 상태: main.c 에 실제로 있고, 다른 화면 파일은 extern 으로 가져다 쓴다 */
extern LibraryData g_data;   /* 도서, 좌석, 학번 명단, 관리자 비밀번호 */
extern int g_is_admin;       /* 0: 관리자 아님, 1: 관리자 모드(관리자가 로그인함) */
extern int g_student_id;     /* 로그인한 학생의 학번. 아무도 로그인하지 않았으면 0 */

/* ===== main.c (권순욱) ===== */
void app_go_to_page(int page);   /* 그 화면으로 이동하고 내용을 새로 고침 */
int  app_acting_student(HWND student_edit);   /* 누구의 학번으로 처리할지 (없으면 -1) */

/* ===== ui.c (권순욱) : 모든 화면이 함께 쓰는 도구 ===== */
void  ui_init(HWND main_window, HINSTANCE instance);
void  ui_free(void);
void  ui_create_sidebar(void);
void  ui_refresh_sidebar(void);
void  ui_show_page(int page);
int   ui_current_page(void);
HWND  ui_main_window(void);
HFONT ui_font(int font_kind);

/* 컨트롤 만들기 (page: 이 컨트롤이 보일 화면 번호) */
HWND ui_label(int page, const wchar_t *text, int x, int y, int w, int h,
              int font_kind, COLORREF color);
HWND ui_edit(int page, int id, int x, int y, int w, int h, int kind);
HWND ui_button(int page, int id, const wchar_t *text, int x, int y, int w, int h);
HWND ui_color_button(int page, int id, int x, int y, int w, int h);
HWND ui_combo(int page, int id, int x, int y, int w, int h);
HWND ui_table(int page, int id, int x, int y, int w, int h);

/* 라벨 글자 바꾸기, 결과 안내 */
void ui_set_label(HWND label, const wchar_t *text, COLORREF color);
void ui_show_result(HWND label, Result result, const wchar_t *success_text);
const wchar_t *ui_result_text(Result result);

/* 표(ListView) 다루기 */
void ui_table_add_column(HWND table, int column, const wchar_t *title, int width);
void ui_table_clear(HWND table);
int  ui_table_add_row(HWND table, const wchar_t *cells[], int cell_count);
int  ui_table_selected_row(HWND table);

/* 한글 문자열 변환과 입력칸 읽기 */
void ui_utf8_to_wide(const char *utf8, wchar_t *wide, int wide_count);
void ui_wide_to_utf8(const wchar_t *wide, char *utf8, int utf8_count);
void ui_get_text(HWND edit, char *utf8, int utf8_count);
int  ui_get_number(HWND edit);
void ui_set_number(HWND edit, int number);
void ui_edit_limit(HWND edit, int max_chars);
void ui_student_text(int student_id, wchar_t *text, int text_count);
void ui_student_input_mode(HWND label, HWND edit);

/* 직접 그리기 */
void ui_paint_sidebar(HDC hdc);
void ui_fill_round_rect(HDC hdc, const RECT *rect, COLORREF back, COLORREF fill);
void ui_draw_text(HDC hdc, const wchar_t *text, RECT *rect, int font_kind,
                  COLORREF color, UINT format);
void ui_draw_box(const DRAWITEMSTRUCT *dis, COLORREF back, COLORREF fill,
                 COLORREF text_color, const wchar_t *text);
void ui_draw_menu_button(const DRAWITEMSTRUCT *dis);
LRESULT ui_static_color(HDC hdc, HWND control);

/* ===== page_home.c (권순욱) : F1 메인 화면 ===== */
void home_create(void);
void home_refresh(void);
void home_command(int id, int code);
void home_draw(const DRAWITEMSTRUCT *dis);
void home_add_log(const wchar_t *text);
void home_set_notice(const wchar_t *text);

/* ===== page_search.c (송영준) : F2 도서 검색 ===== */
void search_page_create(void);
void search_page_refresh(void);
void search_page_command(int id, int code);
void search_page_notify(const NMHDR *header);

/* ===== page_loan.c (유강훈) : F3 대출·반납 ===== */
void loan_page_create(void);
void loan_page_refresh(void);
void loan_page_command(int id, int code);
void loan_page_set_book(int book_id);
void loan_page_reset(void);

/* ===== page_seat.c (류지언) : F4 좌석 예약·취소, F6 시간대 예약 ===== */
void seat_page_create(void);
void seat_page_refresh(void);
void seat_page_command(int id, int code);
void seat_page_draw(const DRAWITEMSTRUCT *dis);
void seat_page_reset(void);

/* ===== page_admin.c (송영준) : F7 관리자 로그인 화면, 관리자 화면 ===== */
void admin_page_create(void);
void admin_page_refresh(void);
void admin_page_command(int id, int code);
void admin_page_focus_login(void);
void admin_page_timer(void);
void admin_logout(void);

/* ===== page_student.c (최민호) : F8 학생 로그인 화면, 내 정보 화면 ===== */
void student_page_create(void);
void student_page_refresh(void);
void student_page_command(int id, int code);
void student_page_focus_login(void);
void student_page_ask_login(void);
void student_logout(void);

#endif
