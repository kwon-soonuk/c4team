/*
 * ui.c  (담당: 권순욱)  모든 화면이 함께 쓰는 Win32 도구 모음
 * ------------------------------------------------------------------
 *   1) 준비와 정리   : 글꼴·붓(brush) 만들기, 지우기
 *   2) 컨트롤 만들기 : 라벨, 입력칸, 버튼, 콤보박스, 표
 *   3) 화면 전환     : 컨트롤마다 "어느 화면 소속인지" 기억했다가 보이기/숨기기
 *   4) 결과 안내     : Result 값을 한글 안내 문구와 색으로 바꾸기
 *   5) 표 다루기     : 열 추가, 줄 추가, 비우기
 *   6) 한글 변환     : 파일의 UTF-8 문자열 ↔ 화면의 wchar_t 문자열, 학번 가리기, 학번 입력칸 상태
 *   7) 직접 그리기   : 왼쪽 메뉴 배경, 둥근 상자, 색 버튼
 *
 * page_*.c 파일은 긴 Win32 코드를 직접 쓰지 않고 여기 함수를 불러서 화면을 만든다.
 */
#include "library.h"
#include "ui.h"
#include <commctrl.h>   /* 표(ListView) */
#include <stdio.h>      /* swprintf */
#include <stdlib.h>     /* wcstol */

/* 표(ListView)를 쓰려면 comctl32 라이브러리를 함께 연결해야 한다 */
#pragma comment(lib, "comctl32.lib")
/* 버튼과 표를 최신 윈도우 모양으로 보이게 하는 설정 (Visual Studio 전용 문법) */
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define MAX_CONTROLS  200   /* 프로그램 전체 컨트롤 최대 개수 */
#define MENU_COUNT    7     /* 왼쪽 메뉴 버튼 수 (홈, 검색, 대출, 좌석, 학생, 관리자, 저장 후 종료) */

/* 컨트롤 하나가 어느 화면에 속하는지 기록하는 구조체 */
typedef struct {
    HWND hwnd;   /* 컨트롤 핸들 (Win32에서 창·컨트롤을 가리키는 번호) */
    int page;    /* 속한 화면 번호 (PAGE_HOME 등, PAGE_ALL 은 항상 보임) */
} PageControl;

/* static 전역 변수: 이 파일 안에서만 쓰는 변수 */
static PageControl g_controls[MAX_CONTROLS];
static int g_control_count = 0;
static HWND g_main_window = NULL;
static HINSTANCE g_instance = NULL;
static int g_current_page = PAGE_HOME;
static HFONT g_fonts[3];
static HBRUSH g_white_brush = NULL;
static HWND g_menu_buttons[MENU_COUNT];

/* ===================== 1) 준비와 정리 ===================== */

static HFONT make_font(int size, int weight)
{
    return CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");   /* 맑은 고딕 */
}

/* 프로그램 시작 시 한 번: 메인 창을 기억하고 글꼴과 붓을 만든다 */
void ui_init(HWND main_window, HINSTANCE instance)
{
    INITCOMMONCONTROLSEX icc;

    g_main_window = main_window;
    g_instance = instance;

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES;   /* 표(ListView)를 쓰겠다고 알림 */
    InitCommonControlsEx(&icc);

    g_fonts[FONT_NORMAL] = make_font(17, FW_NORMAL);
    g_fonts[FONT_BOLD] = make_font(17, FW_BOLD);
    g_fonts[FONT_TITLE] = make_font(30, FW_BOLD);
    g_white_brush = CreateSolidBrush(COLOR_WHITE);
}

/* 프로그램 종료 시: 만든 글꼴과 붓을 돌려준다 (GDI 객체는 직접 지워야 한다) */
void ui_free(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        DeleteObject(g_fonts[i]);
    }
    DeleteObject(g_white_brush);
}

HFONT ui_font(int font_kind)
{
    return g_fonts[font_kind];
}

/* 메인 창 핸들 (메시지 상자를 띄울 때 사용) */
HWND ui_main_window(void)
{
    return g_main_window;
}

/* 지금 보이는 화면 번호 */
int ui_current_page(void)
{
    return g_current_page;
}

/* ===================== 2) 컨트롤 만들기 ===================== */

/*
 * 컨트롤 하나를 만들고, 글꼴을 적용하고, 어느 화면 소속인지 기록한다.
 * 모든 ui_label, ui_button ... 함수가 결국 이 함수를 부른다.
 *   class_name : 컨트롤 종류 (L"STATIC" 라벨, L"EDIT" 입력칸, L"BUTTON" 버튼 ...)
 *   style      : 컨트롤 모양 옵션
 *   id         : WM_COMMAND 로 구분할 번호
 */
static HWND add_control(int page, DWORD ex_style, const wchar_t *class_name,
                        const wchar_t *text, DWORD style, int id,
                        int x, int y, int w, int h, int font_kind)
{
    HWND hwnd = CreateWindowExW(ex_style, class_name, text, WS_CHILD | style,
                                x, y, w, h, g_main_window,
                                (HMENU)(INT_PTR)id,   /* 자식 컨트롤은 이 자리에 ID를 넣는다 */
                                g_instance, NULL);

    SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_fonts[font_kind], TRUE);

    if (g_control_count < MAX_CONTROLS) {
        g_controls[g_control_count].hwnd = hwnd;
        g_controls[g_control_count].page = page;
        g_control_count++;
    }
    return hwnd;
}

/* 글자만 보여주는 라벨. 글자색은 컨트롤의 "사용자 데이터" 칸에 저장해 둔다 */
HWND ui_label(int page, const wchar_t *text, int x, int y, int w, int h,
              int font_kind, COLORREF color)
{
    HWND label = add_control(page, 0, L"STATIC", text, SS_LEFT | SS_NOPREFIX, 0,
                             x, y, w, h, font_kind);

    SetWindowLongPtrW(label, GWLP_USERDATA, (LONG_PTR)color);   /* ui_static_color 에서 꺼내 씀 */
    return label;
}

/*
 * 한 줄 입력칸.
 *   kind : EDIT_TEXT(글자), EDIT_NUMBER(숫자만), EDIT_PASSWORD(글자가 ● 로 보임)
 */
HWND ui_edit(int page, int id, int x, int y, int w, int h, int kind)
{
    DWORD style = WS_TABSTOP | ES_AUTOHSCROLL;
    HWND edit;

    if (kind == EDIT_NUMBER) {
        style = style | ES_NUMBER;
    } else if (kind == EDIT_PASSWORD) {
        style = style | ES_PASSWORD;
    }
    edit = add_control(page, WS_EX_CLIENTEDGE, L"EDIT", L"", style, id, x, y, w, h,
                       FONT_NORMAL);
    if (kind == EDIT_NUMBER) {
        ui_edit_limit(edit, 9);                   /* 최대 9자리 (int 범위를 넘지 않게) */
    } else if (kind == EDIT_PASSWORD) {
        ui_edit_limit(edit, PASSWORD_LEN - 1);    /* 최대 20글자 */
    } else {
        ui_edit_limit(edit, 30);                  /* 최대 30글자 */
    }
    return edit;
}

/* 입력칸에 넣을 수 있는 최대 글자 수를 정한다 */
void ui_edit_limit(HWND edit, int max_chars)
{
    SendMessageW(edit, EM_LIMITTEXT, (WPARAM)max_chars, 0);
}

/* 보통 버튼 */
HWND ui_button(int page, int id, const wchar_t *text, int x, int y, int w, int h)
{
    return add_control(page, 0, L"BUTTON", text, WS_TABSTOP | BS_PUSHBUTTON, id,
                       x, y, w, h, FONT_BOLD);
}

/* 직접 색칠하는 버튼 (BS_OWNERDRAW): 그릴 때마다 WM_DRAWITEM 메시지가 온다 */
HWND ui_color_button(int page, int id, int x, int y, int w, int h)
{
    return add_control(page, 0, L"BUTTON", L"", BS_OWNERDRAW, id, x, y, w, h, FONT_BOLD);
}

/* 목록에서 고르는 콤보박스. h 는 펼쳤을 때 목록 높이까지 포함한 높이 */
HWND ui_combo(int page, int id, int x, int y, int w, int h)
{
    return add_control(page, 0, L"COMBOBOX", L"",
                       WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, id, x, y, w, h,
                       FONT_NORMAL);
}

/* 표(ListView): 여러 칸으로 된 목록 */
HWND ui_table(int page, int id, int x, int y, int w, int h)
{
    HWND table = add_control(page, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                             WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                             id, x, y, w, h, FONT_NORMAL);

    /* 한 줄 전체가 선택되게 + 격자선 보이기 */
    SendMessageW(table, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                 LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    return table;
}

/* 왼쪽 메뉴 버튼 7개를 만든다 (모든 화면에서 보이도록 PAGE_ALL) */
void ui_create_sidebar(void)
{
    const wchar_t *names[6] = { L"홈", L"도서 검색", L"대출 · 반납", L"좌석 예약", L"학생 로그인",
                                L"관리자" };
    int i;

    for (i = 0; i < 6; i++) {
        g_menu_buttons[i] = add_control(PAGE_ALL, 0, L"BUTTON", names[i], BS_OWNERDRAW,
                                        ID_MENU_HOME + i, 16, 126 + i * 52,
                                        SIDEBAR_WIDTH - 32, 44, FONT_BOLD);
    }
    g_menu_buttons[6] = add_control(PAGE_ALL, 0, L"BUTTON", L"저장 후 종료", BS_OWNERDRAW,
                                    ID_MENU_EXIT, 16, WINDOW_HEIGHT - 64,
                                    SIDEBAR_WIDTH - 32, 44, FONT_BOLD);
}

/* 왼쪽 메뉴 영역을 다시 그리게 한다 (학생·관리자가 로그인하거나 로그아웃했을 때) */
void ui_refresh_sidebar(void)
{
    RECT sidebar = { 0, 0, SIDEBAR_WIDTH, WINDOW_HEIGHT };
    int i;

    /* 다섯 번째 메뉴: 학생이 로그인하면 "내 정보", 아니면 "학생 로그인" */
    if (g_student_id != 0) {
        SetWindowTextW(g_menu_buttons[4], L"내 정보");
    } else {
        SetWindowTextW(g_menu_buttons[4], L"학생 로그인");
    }

    InvalidateRect(g_main_window, &sidebar, TRUE);
    for (i = 0; i < MENU_COUNT; i++) {
        InvalidateRect(g_menu_buttons[i], NULL, TRUE);
    }
}

/* ===================== 3) 화면 전환 ===================== */

/* page 화면의 컨트롤만 보이고 나머지는 숨긴다 */
void ui_show_page(int page)
{
    int i;

    g_current_page = page;
    for (i = 0; i < g_control_count; i++) {
        if (g_controls[i].page == PAGE_ALL || g_controls[i].page == page) {
            ShowWindow(g_controls[i].hwnd, SW_SHOW);
        } else {
            ShowWindow(g_controls[i].hwnd, SW_HIDE);
        }
    }
    /* 방금 숨긴 컨트롤에 키보드 입력 위치(포커스)가 남아 있으면 메인 창으로 옮긴다.
     * (숨은 버튼이 Enter 키에 반응하는 일을 막는다) */
    if (GetFocus() != NULL && !IsWindowVisible(GetFocus())) {
        SetFocus(g_main_window);
    }

    /* 메뉴 버튼을 다시 그리게 해서 "지금 화면" 버튼을 파란색으로 바꾼다 */
    for (i = 0; i < MENU_COUNT; i++) {
        InvalidateRect(g_menu_buttons[i], NULL, TRUE);
    }
}

/* ===================== 4) 결과 안내 ===================== */

/* 라벨의 글자와 색을 바꾼다 */
void ui_set_label(HWND label, const wchar_t *text, COLORREF color)
{
    SetWindowLongPtrW(label, GWLP_USERDATA, (LONG_PTR)color);
    SetWindowTextW(label, text);
    InvalidateRect(label, NULL, TRUE);
}

/* 로직 함수의 결과(Result)를 사람이 읽을 안내 문구로 바꾼다 */
const wchar_t *ui_result_text(Result result)
{
    switch (result) {
    case RESULT_OK:            return L"처리했습니다";
    case ERR_INPUT:            return L"입력값을 확인하세요 (빈칸이거나 범위를 벗어났어요)";
    case ERR_NOT_REGISTERED:   return L"등록되지 않은 학번입니다";
    case ERR_BOOK_NOT_FOUND:   return L"없는 도서 번호입니다";
    case ERR_ALREADY_BORROWED: return L"이미 대출 중인 도서라서 대출할 수 없습니다";
    case ERR_NOT_BORROWED:     return L"대출 중이 아닌 도서입니다";
    case ERR_NOT_BORROWER:     return L"대출한 학생만 반납할 수 있습니다";
    case ERR_SEAT_NOT_FOUND:   return L"없는 좌석 번호입니다";
    case ERR_ALREADY_RESERVED: return L"이미 예약된 좌석입니다";
    case ERR_HAS_SEAT:         return L"이 학번은 이미 다른 좌석을 예약했습니다";
    case ERR_NOT_RESERVED:     return L"예약되지 않은 좌석입니다";
    case ERR_NOT_RESERVER:     return L"예약한 학생만 취소할 수 있습니다";
    case ERR_TIME_OVERLAP:     return L"기존 예약과 시간이 겹칩니다. 다른 시간을 고르세요";
    case ERR_TIME_FULL:        return L"이 좌석의 시간대 예약이 가득 찼습니다";
    case ERR_FILE:             return L"파일을 열거나 저장할 수 없습니다";
    case ERR_MEMORY:           return L"메모리가 부족합니다";
    case ERR_DUPLICATE:        return L"이미 있는 번호입니다 (도서 번호나 학번이 겹쳐요)";
    case ERR_IN_USE:           return L"대출이나 예약이 남아 있어서 삭제할 수 없습니다";
    case ERR_FULL:             return L"학번 명단이 가득 찼습니다";
    case ERR_PASSWORD:         return L"비밀번호는 영문·숫자 4~20글자로 정하세요";
    case ERR_LOGIN_FAILED:     return L"비밀번호가 틀렸습니다";
    case ERR_LOGIN_LOCKED:     return L"여러 번 틀려서 잠시 로그인할 수 없습니다";
    case ERR_LOAN_LIMIT:       return L"한 사람이 빌릴 수 있는 10권을 모두 빌렸습니다";
    case ERR_SAME_PASSWORD:    return L"지금 쓰는 비밀번호와 다른 비밀번호로 정하세요";
    default:                   return L"알 수 없는 오류입니다";
    }
}

/* 성공이면 초록색으로 success_text, 실패면 빨간색으로 실패 이유를 보여준다 */
void ui_show_result(HWND label, Result result, const wchar_t *success_text)
{
    if (result == RESULT_OK) {
        ui_set_label(label, success_text, COLOR_GREEN);
    } else {
        ui_set_label(label, ui_result_text(result), COLOR_RED);
    }
}

/* ===================== 5) 표(ListView) 다루기 ===================== */

/* 표에 열(제목 칸)을 하나 추가한다 */
void ui_table_add_column(HWND table, int column, const wchar_t *title, int width)
{
    LVCOLUMNW col = {0};

    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = width;
    col.pszText = (LPWSTR)title;   /* Win32 구조체가 const 를 받지 않아서 형변환 */
    SendMessageW(table, LVM_INSERTCOLUMNW, (WPARAM)column, (LPARAM)&col);
}

/* 표의 모든 줄을 지운다 */
void ui_table_clear(HWND table)
{
    SendMessageW(table, LVM_DELETEALLITEMS, 0, 0);
}

/*
 * 표 맨 아래에 한 줄을 추가하고, 추가한 줄 번호를 돌려준다.
 *   cells      : 칸마다 들어갈 글자 배열 (예: { L"101", L"C언어 기초", ... })
 *   cell_count : 칸 수
 */
int ui_table_add_row(HWND table, const wchar_t *cells[], int cell_count)
{
    LVITEMW item = {0};
    int row = (int)SendMessageW(table, LVM_GETITEMCOUNT, 0, 0);   /* 지금 줄 수 = 새 줄 번호 */
    int i;

    item.mask = LVIF_TEXT;
    item.iItem = row;
    item.iSubItem = 0;
    item.pszText = (LPWSTR)cells[0];
    row = (int)SendMessageW(table, LVM_INSERTITEMW, 0, (LPARAM)&item);   /* 첫 칸으로 새 줄 */

    for (i = 1; i < cell_count; i++) {   /* 나머지 칸 채우기 */
        item.iSubItem = i;
        item.pszText = (LPWSTR)cells[i];
        SendMessageW(table, LVM_SETITEMTEXTW, (WPARAM)row, (LPARAM)&item);
    }
    return row;
}

/* 선택된 줄 번호를 돌려준다 (선택 안 했으면 -1) */
int ui_table_selected_row(HWND table)
{
    return (int)SendMessageW(table, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
}

/* ===================== 6) 한글 문자열 변환 =====================
 * 파일과 로직 코드는 char(UTF-8) 문자열을 쓰고, 윈도우 화면은 wchar_t 문자열을 쓴다.
 * 그래서 화면에 보여줄 때와 입력을 읽을 때 변환이 필요하다. */

void ui_utf8_to_wide(const char *utf8, wchar_t *wide, int wide_count)
{
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wide_count) == 0) {
        wide[0] = L'\0';   /* 변환에 실패하면 빈 문자열 */
    }
}

void ui_wide_to_utf8(const wchar_t *wide, char *utf8, int utf8_count)
{
    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8_count, NULL, NULL) == 0) {
        utf8[0] = '\0';
    }
}

/* 입력칸의 글자를 UTF-8 문자열로 읽는다 */
void ui_get_text(HWND edit, char *utf8, int utf8_count)
{
    wchar_t wide[100];

    GetWindowTextW(edit, wide, 100);
    ui_wide_to_utf8(wide, utf8, utf8_count);
}

/* 입력칸의 숫자를 정수로 읽는다 (비어 있으면 0) */
int ui_get_number(HWND edit)
{
    wchar_t text[20];

    GetWindowTextW(edit, text, 20);
    return (int)wcstol(text, NULL, 10);   /* 문자열 → 정수 (wchar_t 용 atoi) */
}

void ui_set_number(HWND edit, int number)
{
    wchar_t text[20];

    swprintf(text, 20, L"%d", number);
    SetWindowTextW(edit, text);
}

/*
 * 학번을 화면에 보여줄 글자로 바꾼다.
 *   관리자 모드, 또는 로그인한 학생 자신의 학번 : 그대로            (예: 20261234)
 *   그 밖의 경우(다른 학생의 학번)              : 뒤 4자리를 가린다 (예: 2026****)
 */
void ui_student_text(int student_id, wchar_t *text, int text_count)
{
    if (g_is_admin || student_id == g_student_id) {
        swprintf(text, text_count, L"%d", student_id);
    } else {
        swprintf(text, text_count, L"%d****", student_id / 10000);
    }
}

/*
 * 대출·좌석 화면의 "학번" 라벨과 입력칸을 지금 상태에 맞게 바꾼다.
 *   관리자 모드 : 입력칸을 보여 준다 (학생 대신 처리할 학번을 적는다)
 *   학생 로그인 : 입력칸을 숨기고 로그인한 학번을 보여 준다
 *   로그인 안 함: 입력칸을 숨기고 로그인하라는 안내를 보여 준다
 */
void ui_student_input_mode(HWND label, HWND edit)
{
    wchar_t text[60];

    if (g_is_admin) {
        ui_set_label(label, L"학번 (관리자: 대신 처리할 학번)", COLOR_GRAY);
        ShowWindow(edit, SW_SHOW);
    } else if (g_student_id != 0) {
        swprintf(text, 60, L"로그인한 학번: %d", g_student_id);
        ui_set_label(label, text, COLOR_BLUE);
        ShowWindow(edit, SW_HIDE);
    } else {
        ui_set_label(label, L"학생 로그인 후 이용할 수 있습니다", COLOR_GRAY);
        ShowWindow(edit, SW_HIDE);
    }
}

/* ===================== 7) 직접 그리기 ===================== */

/* 색을 조금 어둡게 만든다 (버튼이 눌렸을 때 사용) */
static COLORREF darker(COLORREF color)
{
    return RGB(GetRValue(color) * 85 / 100, GetGValue(color) * 85 / 100,
               GetBValue(color) * 85 / 100);
}

/* 글자를 rect 영역 안에 그린다 (format: DT_LEFT, DT_CENTER, DT_SINGLELINE 등) */
void ui_draw_text(HDC hdc, const wchar_t *text, RECT *rect, int font_kind,
                  COLORREF color, UINT format)
{
    HGDIOBJ old_font = SelectObject(hdc, g_fonts[font_kind]);

    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);   /* 글자 뒤 배경은 칠하지 않음 */
    DrawTextW(hdc, text, -1, rect, format);
    SelectObject(hdc, old_font);
}

/* 모서리가 둥근 상자를 그린다. 둥근 모서리 바깥은 back 색으로 칠한다 */
void ui_fill_round_rect(HDC hdc, const RECT *rect, COLORREF back, COLORREF fill)
{
    HBRUSH back_brush = CreateSolidBrush(back);
    HBRUSH fill_brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, fill);
    HGDIOBJ old_brush, old_pen;

    FillRect(hdc, rect, back_brush);
    old_brush = SelectObject(hdc, fill_brush);
    old_pen = SelectObject(hdc, pen);
    RoundRect(hdc, rect->left, rect->top, rect->right, rect->bottom, 16, 16);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);

    DeleteObject(back_brush);   /* 만든 GDI 객체는 다 쓰면 꼭 지운다 */
    DeleteObject(fill_brush);
    DeleteObject(pen);
}

/* 색 버튼 하나를 그린다: 둥근 상자 + 가운데 글자 (여러 줄 가능, \n 으로 줄바꿈) */
void ui_draw_box(const DRAWITEMSTRUCT *dis, COLORREF back, COLORREF fill,
                 COLORREF text_color, const wchar_t *text)
{
    RECT rect = dis->rcItem;
    RECT text_rect;
    int text_height;

    if (dis->itemState & ODS_SELECTED) {   /* 누르고 있는 동안은 조금 어둡게 */
        fill = darker(fill);
    }
    ui_fill_round_rect(dis->hDC, &rect, back, fill);

    /* 글자를 세로 가운데에 놓기: 먼저 글자 높이를 재고(DT_CALCRECT) 그만큼 내려서 그린다 */
    text_rect = rect;
    ui_draw_text(dis->hDC, text, &text_rect, FONT_BOLD, text_color,
                 DT_CENTER | DT_WORDBREAK | DT_CALCRECT);
    text_height = text_rect.bottom - text_rect.top;

    text_rect = rect;
    text_rect.top = rect.top + (rect.bottom - rect.top - text_height) / 2;
    ui_draw_text(dis->hDC, text, &text_rect, FONT_BOLD, text_color, DT_CENTER | DT_WORDBREAK);
}

/* 왼쪽 메뉴 버튼 그리기: 지금 화면은 파란색, 나머지는 남색 */
void ui_draw_menu_button(const DRAWITEMSTRUCT *dis)
{
    wchar_t text[30];
    int id = (int)dis->CtlID;
    COLORREF fill = COLOR_NAVY;
    COLORREF text_color = COLOR_SIDEBAR_TEXT;

    GetWindowTextW(dis->hwndItem, text, 30);
    if (id == ID_MENU_EXIT) {
        fill = COLOR_NAVY_LIGHT;
        text_color = COLOR_WHITE;
    } else if (id - ID_MENU_HOME == g_current_page
               || (id == ID_MENU_ADMIN && g_current_page == PAGE_LOGIN)
               || (id == ID_MENU_STUDENT && g_current_page == PAGE_STUDENT_LOGIN)) {
        fill = COLOR_BLUE;   /* 로그인 화면에서도 해당 메뉴를 파랗게 */
        text_color = COLOR_WHITE;
    }
    ui_draw_box(dis, COLOR_NAVY, fill, text_color, text);
}

/* 왼쪽 남색 메뉴 영역, 프로그램 이름, 지금 모드를 그린다 (WM_PAINT 에서 호출) */
void ui_paint_sidebar(HDC hdc)
{
    RECT sidebar = { 0, 0, SIDEBAR_WIDTH, WINDOW_HEIGHT };
    RECT title = { 22, 26, SIDEBAR_WIDTH - 8, 52 };
    RECT sub = { 22, 54, SIDEBAR_WIDTH - 8, 78 };
    RECT mode = { 22, 86, SIDEBAR_WIDTH - 8, 110 };
    HBRUSH brush = CreateSolidBrush(COLOR_NAVY);

    FillRect(hdc, &sidebar, brush);
    DeleteObject(brush);
    ui_draw_text(hdc, L"도서관 관리 프로그램", &title, FONT_BOLD, COLOR_WHITE,
                 DT_LEFT | DT_SINGLELINE);
    ui_draw_text(hdc, L"컴퓨터공학과 자료실", &sub, FONT_NORMAL, COLOR_SIDEBAR_TEXT,
                 DT_LEFT | DT_SINGLELINE);
    if (g_is_admin) {
        ui_draw_text(hdc, L"● 관리자 모드", &mode, FONT_BOLD, COLOR_ADMIN_MODE,
                     DT_LEFT | DT_SINGLELINE);
    } else if (g_student_id != 0) {
        wchar_t text[40];

        swprintf(text, 40, L"● %d 로그인 중", g_student_id);
        ui_draw_text(hdc, text, &mode, FONT_BOLD, COLOR_WHITE, DT_LEFT | DT_SINGLELINE);
    } else {
        ui_draw_text(hdc, L"● 로그인하지 않음", &mode, FONT_BOLD, COLOR_SIDEBAR_TEXT,
                     DT_LEFT | DT_SINGLELINE);
    }
}

/*
 * 라벨(STATIC)을 그리기 직전에 윈도우가 "무슨 색으로 그릴까?" 물어본다(WM_CTLCOLORSTATIC).
 * ui_label 에서 저장해 둔 글자색을 꺼내서 알려주고, 배경은 흰색으로 한다.
 */
LRESULT ui_static_color(HDC hdc, HWND control)
{
    COLORREF color = (COLORREF)GetWindowLongPtrW(control, GWLP_USERDATA);

    SetTextColor(hdc, color);
    SetBkColor(hdc, COLOR_WHITE);
    return (LRESULT)g_white_brush;
}
