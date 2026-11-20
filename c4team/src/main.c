/*
 * main.c  (담당: 권순욱)  프로그램 시작점과 메시지 처리
 * ------------------------------------------------------------------
 * 윈도우 프로그램의 흐름
 *   1) WinMain     : data 폴더 찾기 → 파일에서 불러오기 → 창 만들기 → 화면 8개 만들기
 *   2) 메시지 반복 : 사용자가 클릭·입력할 때마다 윈도우가 "메시지"를 보내 준다
 *   3) window_proc : 메시지 종류를 보고 알맞은 화면 파일(page_*.c)의 함수를 부른다
 *   4) 종료        : 관리자만 가능. [저장 후 종료] 또는 X 버튼 → 저장 → 메모리 정리
 *
 * 세 가지 상태
 *   - 아무도 로그인하지 않음 (g_student_id = 0, g_is_admin = 0): 도서 검색과 좌석 현황만 볼 수 있다
 *   - 학생 로그인 (g_student_id = 학번): 자기 학번으로 대출·반납·좌석 예약을 한다
 *   - 관리자 로그인 (g_is_admin = 1): 관리자 화면을 쓰고, 학번을 입력해 학생 대신 처리할 수 있다
 * 학생이 AUTO_LOGOUT_SECONDS(60)초 동안 아무것도 하지 않으면 자동으로 로그아웃한다.
 */
#include "library.h"
#include "ui.h"
#include <wchar.h>   /* wcsrchr */

/* 프로그램 전체 상태. 다른 파일은 ui.h 의 extern 으로 사용한다 */
LibraryData g_data;    /* 도서, 좌석, 학생 계정 명단, 관리자 비밀번호 */
int g_is_admin = 0;    /* 0: 관리자 아님, 1: 관리자 모드 */
int g_student_id = 0;  /* 로그인한 학생의 학번 (없으면 0) */

static DWORD g_last_input = 0;   /* 마지막으로 키보드·마우스를 쓴 시각 (GetTickCount 값, 밀리초) */

/* 화면 내용을 최신 데이터로 새로 고친 뒤 그 화면을 보여준다 */
void app_go_to_page(int page)
{
    /* 로그인하지 않았는데 관리자 화면·내 정보 화면으로 가려고 하면 로그인 화면으로 보낸다 */
    if (page == PAGE_ADMIN && !g_is_admin) {
        page = PAGE_LOGIN;
    }
    if (page == PAGE_STUDENT && g_student_id == 0) {
        page = PAGE_STUDENT_LOGIN;
    }
    /* 임시 비밀번호로 로그인한 학생은 비밀번호를 바꿀 때까지 내 정보 화면만 볼 수 있다 */
    if (g_student_id != 0 && must_change_password(&g_data, g_student_id)) {   /* user.c */
        page = PAGE_STUDENT;
    }

    /* 먼저 그 화면의 컨트롤을 모두 보이게 한 다음, 화면별 새로 고침에서
     * 지금 상태(학생/관리자)에 필요 없는 컨트롤을 다시 숨긴다 */
    ui_show_page(page);

    if (page == PAGE_HOME) {
        home_refresh();
    } else if (page == PAGE_SEARCH) {
        search_page_refresh();
    } else if (page == PAGE_LOAN) {
        loan_page_refresh();
    } else if (page == PAGE_SEAT) {
        seat_page_refresh();
    } else if (page == PAGE_ADMIN || page == PAGE_LOGIN) {
        admin_page_refresh();
    } else if (page == PAGE_STUDENT || page == PAGE_STUDENT_LOGIN) {
        student_page_refresh();
    }

    if (page == PAGE_LOGIN) {
        admin_page_focus_login();     /* 바로 비밀번호를 칠 수 있게 입력칸에 커서를 둔다 */
    } else if (page == PAGE_STUDENT_LOGIN) {
        student_page_focus_login();
    }
}

/*
 * 대출·반납·좌석 예약을 "누구의 학번으로" 처리할지 정한다.
 *   관리자 모드 : 학번 입력칸에 적힌 학번 (학생 대신 처리)
 *   학생 로그인 : 로그인한 학생의 학번
 *   로그인 안 함: 학생 로그인 화면으로 보내고 -1 을 돌려준다
 */
int app_acting_student(HWND student_edit)
{
    if (g_is_admin) {
        return ui_get_number(student_edit);
    }
    if (g_student_id != 0) {
        return g_student_id;
    }
    student_page_ask_login();   /* page_student.c : "로그인 후 이용할 수 있습니다" */
    return -1;
}

/* 1초마다(WM_TIMER): 학생이 오랫동안 아무것도 하지 않았으면 자동으로 로그아웃한다 */
static void check_idle_logout(void)
{
    DWORD idle_seconds = (GetTickCount() - g_last_input) / 1000;   /* 밀리초 → 초 */

    if (g_student_id != 0 && idle_seconds >= AUTO_LOGOUT_SECONDS) {
        student_logout();   /* page_student.c */
        home_set_notice(L"오랫동안 사용하지 않아 자동으로 로그아웃했습니다");
        app_go_to_page(PAGE_HOME);
    }
}

/*
 * 관리자 모드이면 1을 돌려준다.
 * 이용자 모드이면 안내한 뒤 로그인 화면으로 보내고 0을 돌려준다.
 */
static int require_admin(HWND hwnd)
{
    if (g_is_admin) {
        return 1;
    }
    MessageBoxW(hwnd, L"프로그램 종료는 관리자만 할 수 있습니다.\n관리자 로그인 화면으로 이동합니다.",
                L"관리자 전용", MB_OK | MB_ICONINFORMATION);
    app_go_to_page(PAGE_LOGIN);
    return 0;
}

/* F5: 저장하고 종료한다. 저장에 실패하면 안내 후 홈 화면으로 돌아간다 */
static void save_and_exit(HWND hwnd)
{
    if (save_all(&g_data) == RESULT_OK) {
        MessageBoxW(hwnd, L"data 폴더에 저장했습니다.\n다음에 실행하면 지금 상태 그대로 불러옵니다.",
                    L"저장 완료", MB_OK | MB_ICONINFORMATION);
        DestroyWindow(hwnd);   /* 창을 닫으면 WM_DESTROY 메시지가 온다 */
    } else {
        MessageBoxW(hwnd, L"저장에 실패했습니다.\n홈 화면으로 돌아갑니다.",
                    L"저장 실패", MB_OK | MB_ICONERROR);
        app_go_to_page(PAGE_HOME);
    }
}

/* 창의 X 버튼을 눌렀을 때: 저장할지 물어본다 */
static void ask_before_close(HWND hwnd)
{
    int answer = MessageBoxW(hwnd, L"종료하기 전에 저장할까요?", L"종료",
                             MB_YESNOCANCEL | MB_ICONQUESTION);

    if (answer == IDYES) {
        save_and_exit(hwnd);
    } else if (answer == IDNO) {
        DestroyWindow(hwnd);   /* 저장하지 않고 종료 */
    }
    /* IDCANCEL(취소)이면 아무것도 하지 않고 프로그램을 계속 쓴다 */
}

/* Enter 키를 눌렀을 때: 검색 화면이면 검색, 로그인 화면이면 로그인, 내 정보 화면이면 비밀번호 변경 */
static void on_enter_key(void)
{
    if (ui_current_page() == PAGE_SEARCH) {
        search_page_command(ID_SEARCH_BUTTON, BN_CLICKED);
    } else if (ui_current_page() == PAGE_LOGIN) {
        admin_page_command(ID_LOGIN_BUTTON, BN_CLICKED);
    } else if (ui_current_page() == PAGE_STUDENT_LOGIN) {
        student_page_command(ID_STUDENT_LOGIN, BN_CLICKED);
    } else if (ui_current_page() == PAGE_STUDENT) {
        student_page_command(ID_STUDENT_PW_CHANGE, BN_CLICKED);
    }
}

/* 버튼 클릭, 입력칸 변경 등(WM_COMMAND)을 ID 범위로 나눠 화면 파일에 전달한다 */
static void on_command(HWND hwnd, int id, int code)
{
    if (id == IDOK) {                        /* Enter 키 (메시지 반복의 IsDialogMessageW 가 보내 준다) */
        on_enter_key();
    } else if (id >= ID_MENU_HOME && id <= ID_MENU_ADMIN) {
        app_go_to_page(id - ID_MENU_HOME);   /* 메뉴 ID - 10 = 화면 번호 */
    } else if (id == ID_MENU_EXIT) {
        if (require_admin(hwnd)) {
            save_and_exit(hwnd);
        }
    } else if (id >= 100 && id < 200) {
        home_command(id, code);
    } else if (id >= 200 && id < 300) {
        search_page_command(id, code);
    } else if (id >= 300 && id < 400) {
        loan_page_command(id, code);
    } else if (id >= 400 && id < 500) {
        seat_page_command(id, code);
    } else if (id >= 500 && id < 600) {
        admin_page_command(id, code);
    } else if (id >= 600 && id < 700) {
        student_page_command(id, code);
    }
}

/* 색 버튼(BS_OWNERDRAW)을 그려야 할 때(WM_DRAWITEM) 알맞은 그리기 함수를 부른다 */
static void on_draw_item(const DRAWITEMSTRUCT *dis)
{
    int id = (int)dis->CtlID;

    if (id >= ID_MENU_HOME && id <= ID_MENU_EXIT) {
        ui_draw_menu_button(dis);
    } else if (id >= ID_HOME_TILE_SEARCH && id <= ID_HOME_TILE_SEAT) {
        home_draw(dis);
    } else if (id >= ID_SEAT_FIRST && id <= ID_SEAT_LAST) {
        seat_page_draw(dis);
    }
}

/*
 * 윈도우 프로시저: 윈도우가 보내는 모든 메시지가 이 함수로 들어온다.
 *   msg    : 메시지 종류 (WM_COMMAND, WM_PAINT ...)
 *   wParam, lParam : 메시지마다 다른 추가 정보
 */
static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:   /* 버튼 클릭, 입력칸 글자 변경 등 */
        on_command(hwnd, LOWORD(wParam), HIWORD(wParam));   /* LOWORD: ID, HIWORD: 알림 코드 */
        return 0;

    case WM_NOTIFY:    /* 표(ListView)에서 온 알림 (예: 두 번 클릭) */
        search_page_notify((const NMHDR *)lParam);
        return 0;

    case WM_DRAWITEM:  /* 색 버튼을 그려야 할 때 */
        on_draw_item((const DRAWITEMSTRUCT *)lParam);
        return TRUE;

    case WM_CTLCOLORSTATIC:   /* 라벨을 그리기 직전: 글자색과 배경색 정하기 */
        return ui_static_color((HDC)wParam, (HWND)lParam);

    case WM_PAINT: {   /* 창을 다시 그려야 할 때: 왼쪽 남색 메뉴 영역 그리기 */
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        ui_paint_sidebar(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER:     /* SetTimer 로 맞춰 둔 시간이 될 때마다 온다 (로그인 잠금 시간 세기) */
        if (wParam == TIMER_LOGIN_LOCK) {
            admin_page_timer();
        } else if (wParam == TIMER_IDLE) {
            check_idle_logout();
        }
        return 0;

    case WM_CLOSE:     /* X 버튼 */
        if (require_admin(hwnd)) {
            ask_before_close(hwnd);
        }
        return 0;

    case WM_DESTROY:   /* 창이 닫힘 → 메시지 반복을 끝내라고 알림 */
        PostQuitMessage(0);
        return 0;
    }
    /* 우리가 처리하지 않는 메시지는 윈도우 기본 처리에 맡긴다 */
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* 지금 폴더에 data 폴더가 있으면 1, 없으면 0 */
static int data_folder_exists(void)
{
    return GetFileAttributesW(L"data") != INVALID_FILE_ATTRIBUTES;
}

/*
 * data 폴더가 있는 곳을 "작업 폴더"로 정한다. (file.c 는 작업 폴더 기준으로 data/... 를 연다)
 * Visual Studio 설정이나 실행 방법이 달라도 같은 data 폴더를 찾기 위한 함수.
 *   1) 지금 작업 폴더에 data 가 있으면 그대로 쓴다
 *   2) 없으면 실행 파일(.exe)이 있는 폴더부터 위로 3단계까지 올라가며 찾는다
 *      (예: c4team\x64\Debug\LibraryProgram.exe → c4team\data)
 *   3) 그래도 없으면 실행 파일이 있는 폴더를 쓴다 (저장할 때 data 폴더를 새로 만든다)
 */
static void find_data_folder(void)
{
    wchar_t exe_folder[MAX_PATH];
    wchar_t *last_slash;
    int i;

    if (data_folder_exists()) {
        return;
    }

    GetModuleFileNameW(NULL, exe_folder, MAX_PATH);   /* 실행 파일의 전체 경로 */
    last_slash = wcsrchr(exe_folder, L'\\');          /* 마지막 \ 의 위치 */
    if (last_slash == NULL) {
        return;
    }
    *last_slash = L'\0';                              /* 파일 이름을 잘라내 폴더 경로만 남긴다 */

    SetCurrentDirectoryW(exe_folder);
    for (i = 0; i < 3; i++) {
        if (data_folder_exists()) {
            return;
        }
        SetCurrentDirectoryW(L"..");                  /* 한 단계 위 폴더로 */
    }
    SetCurrentDirectoryW(exe_folder);
}

/*
 * 윈도우 프로그램의 main 함수
 * (_In_ 같은 표시는 Visual Studio 경고 C28251 을 없애기 위한 것으로, 동작에는 영향 없음)
 */
int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance,
                   _In_ LPSTR command_line, _In_ int show_command)
{
    WNDCLASSW wc = {0};
    /* 크기 고정 창. WS_CLIPCHILDREN: 창 배경을 칠할 때 버튼·입력칸 위는 칠하지 않는다 */
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    int width, height, x, y;
    HWND hwnd;
    MSG msg;
    Result load_result;

    (void)prev_instance;   /* 쓰지 않는 매개변수 */
    (void)command_line;

    /* 1) data 폴더를 찾고 파일에서 데이터 불러오기 (F5) */
    find_data_folder();
    load_result = load_all(&g_data);

    /* 2) 창 종류 등록 → 화면 가운데에 창 만들기 */
    wc.lpfnWndProc = window_proc;               /* 메시지를 받을 함수 */
    wc.hInstance = instance;
    wc.lpszClassName = L"LibraryProgramWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);   /* 배경 흰색 */
    RegisterClassW(&wc);

    AdjustWindowRect(&rect, style, FALSE);      /* 제목 표시줄까지 포함한 창 크기 계산 */
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;    /* (화면 너비 - 창 너비) / 2 */
    y = (GetSystemMetrics(SM_CYSCREEN) - height) / 3;   /* 가운데보다 조금 위쪽 */
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    hwnd = CreateWindowW(L"LibraryProgramWindow", L"도서관 관리 프로그램 - 4팀", style,
                         x, y, width, height, NULL, NULL, instance, NULL);
    if (hwnd == NULL) {
        free_all(&g_data);
        return 1;
    }

    /* 3) 공통 도구 준비 → 왼쪽 메뉴와 화면 8개의 컨트롤 만들기 */
    ui_init(hwnd, instance);
    ui_create_sidebar();
    home_create();
    search_page_create();
    loan_page_create();
    seat_page_create();
    student_page_create();
    admin_page_create();

    if (load_result == RESULT_OK) {
        home_add_log(L"data 폴더의 저장 파일을 불러왔습니다");
    } else {
        home_add_log(L"저장 파일이 없어 초기 데이터로 시작했습니다");
    }
    app_go_to_page(PAGE_HOME);

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    g_last_input = GetTickCount();
    SetTimer(hwnd, TIMER_IDLE, 1000, NULL);   /* 1초마다 WM_TIMER : 자동 로그아웃 확인 */

    /* 4) 메시지 반복: 창이 닫힐 때까지 메시지를 꺼내서 window_proc 으로 보낸다 */
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        /* 키보드나 마우스를 쓸 때마다 시각을 기록한다 (자동 로그아웃 계산에 사용) */
        if (msg.message == WM_KEYDOWN || msg.message == WM_LBUTTONDOWN
            || msg.message == WM_MOUSEWHEEL) {
            g_last_input = GetTickCount();
        }
        /* IsDialogMessageW: Tab 키로 입력칸 이동, Enter 키(IDOK)를 처리해 준다.
         * 처리하지 않은 메시지(0을 돌려줌)만 아래에서 직접 보낸다 */
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);   /* 키보드 입력을 글자 메시지로 바꿈 (한글 입력 포함) */
            DispatchMessageW(&msg);   /* window_proc 호출 */
        }
    }

    /* 5) 정리 */
    ui_free();
    free_all(&g_data);
    return 0;
}
