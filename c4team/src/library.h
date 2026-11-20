/*
 * library.h  (담당: 권순욱)
 * ------------------------------------------------------------------
 * 모든 팀원이 함께 쓰는 "약속"을 모아 둔 헤더 파일
 *   1) 상수        : 좌석 수, 운영 시간, 문자열 길이, 학번 범위
 *   2) 결과 코드   : 함수가 성공했는지, 실패했다면 왜 실패했는지
 *   3) 구조체      : Book(도서), TimeSlot(시간대 예약), Seat(좌석), LibraryData(전체 데이터)
 *   4) 함수 선언   : user.c, search.c, loan.c, seat.c, file.c, admin.c, crypto.c 에 있는 로직 함수
 *
 * 규칙 1. 모든 .c 파일은 이 파일을 "가장 먼저" include 한다.
 *         (아래 _CRT_SECURE_NO_WARNINGS 가 다른 헤더보다 먼저 와야 효과가 있다)
 * 규칙 2. 이 파일을 고칠 때는 단톡에 먼저 공유한다. (모든 팀원 코드에 영향)
 * 규칙 3. 이 파일과 로직 파일에는 Win32(화면) 코드를 넣지 않는다.
 */
#ifndef LIBRARY_H
#define LIBRARY_H

/* Visual Studio에서 fopen, sscanf, wcscpy 등을 쓸 때 나는 C4996 오류를 끈다 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* ===== 1) 상수 ===== */
#define TITLE_LEN       80   /* 제목 최대 바이트 수 (UTF-8 한글은 한 글자에 3바이트) */
#define AUTHOR_LEN      40   /* 저자 최대 바이트 수 */
#define MAX_USERS       200  /* 등록 학번 최대 개수 */
#define SEAT_COUNT      12   /* 열람 좌석 수 */
#define MAX_TIME_SLOTS  10   /* 좌석 하나당 시간대 예약 최대 개수 (F6) */
#define OPEN_HOUR       9    /* 자료실 여는 시각 (9시) */
#define CLOSE_HOUR      18   /* 자료실 닫는 시각 (18시) */
#define FIRST_CAPACITY  10   /* 도서 배열의 처음 크기 (꽉 차면 2배로 늘린다) */

#define STUDENT_ID_MIN  10000000   /* 학번은 8자리 숫자 (새로 등록할 때 검사) */
#define STUDENT_ID_MAX  99999999

#define MAX_LOANS       10   /* 한 학생이 동시에 빌릴 수 있는 도서 수 */

#define PASSWORD_LEN      21            /* 관리자 비밀번호: 최대 20글자 + 문자열 끝 표시 */
#define PASSWORD_MIN      4             /* 관리자 비밀번호 최소 글자 수 */
#define DEFAULT_PASSWORD  "admin1234"   /* admin.txt 가 없을 때 쓰는 처음 비밀번호 */
#define SALT_HEX_LEN      33            /* 솔트: 16진수 32글자 + 문자열 끝 표시 */
#define HASH_HEX_LEN      65            /* 해시: 16진수 64글자 + 문자열 끝 표시 */
#define HASH_ITERATIONS   20000         /* 해시 계산을 되풀이하는 횟수 (crypto.c) */
#define HASH_NAME         "pbkdf2-sha256"   /* admin.txt 맨 앞에 쓰는 해시 방식 이름 */
#define LOGIN_MAX_FAILS   5             /* 이만큼 연속으로 틀리면 로그인을 잠시 막는다 */
#define LOGIN_LOCK_SECONDS 10           /* 로그인을 막는 시간(초) */
#define TEMP_PASSWORD_LEN  9             /* 임시 비밀번호: 8글자 + 문자열 끝 표시 */
#define AUTO_LOGOUT_SECONDS 60          /* 학생이 이 시간(초) 동안 아무것도 안 하면 자동 로그아웃 */

/* ===== 2) 결과 코드 =====
 * 로직 함수는 성공하면 RESULT_OK, 실패하면 "왜 실패했는지"를 돌려준다.
 * 화면 코드(ui.c의 ui_result_text)가 이 값을 보고 알맞은 안내 문구를 보여준다. */
typedef enum {
    RESULT_OK = 0,
    ERR_INPUT,             /* 입력값이 비었거나 범위를 벗어남 */
    ERR_NOT_REGISTERED,    /* 등록되지 않은 학번 */
    ERR_BOOK_NOT_FOUND,    /* 없는 도서 번호 */
    ERR_ALREADY_BORROWED,  /* 이미 대출 중인 도서 */
    ERR_NOT_BORROWED,      /* 대출 중이 아닌 도서를 반납하려 함 */
    ERR_NOT_BORROWER,      /* 대출한 학생이 아닌 사람이 반납하려 함 */
    ERR_SEAT_NOT_FOUND,    /* 없는 좌석 번호 */
    ERR_ALREADY_RESERVED,  /* 이미 예약된 좌석 */
    ERR_HAS_SEAT,          /* 이미 다른 좌석을 예약한 학번 */
    ERR_NOT_RESERVED,      /* 예약되지 않은 좌석을 취소하려 함 */
    ERR_NOT_RESERVER,      /* 예약한 학생이 아닌 사람이 취소하려 함 */
    ERR_TIME_OVERLAP,      /* 기존 시간대 예약과 겹침 (F6) */
    ERR_TIME_FULL,         /* 이 좌석의 시간대 예약이 가득 참 (F6) */
    ERR_FILE,              /* 파일을 열거나 쓸 수 없음 (F5) */
    ERR_MEMORY,            /* 메모리 부족 (도서 동적 배열) */
    ERR_DUPLICATE,         /* 이미 있는 도서 번호 / 이미 등록된 학번 (F7) */
    ERR_IN_USE,            /* 대출·예약이 남아 있어서 삭제할 수 없음 (F7) */
    ERR_FULL,              /* 학번 명단이 가득 참 (F7) */
    ERR_PASSWORD,          /* 비밀번호 형식이 맞지 않음 (F7) */
    ERR_LOGIN_FAILED,      /* 관리자 비밀번호가 틀림 (F7) */
    ERR_LOGIN_LOCKED,      /* 여러 번 틀려서 로그인이 잠시 막힘 (F7) */
    ERR_LOAN_LIMIT,        /* 한 학생이 빌릴 수 있는 권수(MAX_LOANS)를 넘음 (F3) */
    ERR_SAME_PASSWORD      /* 새 비밀번호가 지금 비밀번호와 같음 (F8) */
} Result;

/* ===== 3) 구조체 ===== */
typedef enum { BOOK_AVAILABLE = 0, BOOK_BORROWED = 1 } BookState;
typedef enum { SEAT_EMPTY = 0, SEAT_RESERVED = 1 } SeatState;

/* 도서 한 권 */
typedef struct {
    int id;                    /* 도서 번호 (예: 101) */
    char title[TITLE_LEN];     /* 제목 (UTF-8 문자열) */
    char author[AUTHOR_LEN];   /* 저자 */
    BookState state;           /* 대출 가능 / 대출 중 */
    int borrower_id;           /* 대출한 학번 (대출 중이 아니면 0) */
} Book;

/* 시간대 예약 한 건 (F6) */
typedef struct {
    int start_hour;            /* 시작 시각 (13 이면 13시) */
    int end_hour;              /* 종료 시각 (15 이면 15시) */
    int student_id;            /* 예약한 학번 */
} TimeSlot;

/* 열람 좌석 한 개 */
typedef struct {
    int number;                        /* 좌석 번호 1 ~ 12 */
    SeatState state;                   /* 지금 비어 있음 / 예약 중 (F4) */
    int reserver_id;                   /* 예약한 학번 (비어 있으면 0) */
    TimeSlot slots[MAX_TIME_SLOTS];    /* 오늘의 시간대 예약 목록 (F6), 시작 시각 순 */
    int slot_count;                    /* 시간대 예약 개수 */
} Seat;

/* 학생 계정 한 개 (F8). 비밀번호 자체는 저장하지 않고 솔트와 해시만 가진다 */
typedef struct {
    int id;                    /* 학번 (8자리) */
    char salt[SALT_HEX_LEN];   /* 비밀번호의 솔트 */
    char hash[HASH_HEX_LEN];   /* 비밀번호의 해시 */
    int must_change;           /* 1: 임시 비밀번호라서 로그인 후 바꿔야 함, 0: 직접 정한 비밀번호 */
} User;

/* 프로그램 전체 데이터를 하나로 묶은 구조체 (main.c 에 딱 하나 만든다) */
typedef struct {
    Book *books;               /* 도서 동적 배열 (도전 목표: realloc 으로 늘린다) */
    int book_count;            /* 실제로 들어 있는 도서 수 */
    int book_capacity;         /* 지금 배열에 넣을 수 있는 최대 도서 수 */
    Seat seats[SEAT_COUNT];    /* 좌석 12개 */
    User users[MAX_USERS];     /* 등록된 학생 계정 명단 */
    int user_count;            /* 등록된 학생 수 */
    char admin_salt[SALT_HEX_LEN];   /* 관리자 비밀번호의 솔트 (F7) */
    char admin_hash[HASH_HEX_LEN];   /* 관리자 비밀번호의 해시. 비밀번호 자체는 어디에도 저장하지 않는다 */
} LibraryData;

/* 로그인 실패 횟수와 잠금 상태 (F7). 프로그램이 켜져 있는 동안만 기억한다 */
typedef struct {
    int fail_count;         /* 연속으로 틀린 횟수 (성공하면 0으로) */
    long long locked_until; /* 이 시각(time 함수의 초)까지 로그인 불가. 잠겨 있지 않으면 0 */
} LoginGuard;

/* ===== 4) 로직 함수 선언 ===== */

/* user.c (최민호) : F8 학생 계정 - 등록 확인, 학생 로그인, 비밀번호 변경 */
int find_user_index(const User users[], int user_count, int student_id);
int is_registered(const User users[], int user_count, int student_id);
void set_user_password(User *user, const char *password, int must_change);
int check_user_password(const User *user, const char *input);
Result login_student(const LibraryData *data, LoginGuard *guard, int student_id,
                     const char *input, long long now, int *wait_seconds);
int must_change_password(const LibraryData *data, int student_id);
Result change_student_password(LibraryData *data, int student_id, const char *new_password);

/* search.c (송영준) : F2 도서 검색 */
int search_books(const Book books[], int book_count, const char *keyword,
                 int found[], int max_found);
int find_book_index(const Book books[], int book_count, int book_id);

/* loan.c (유강훈) : F3 도서 대출·반납 */
Result borrow_book(Book books[], int book_count, int book_id, int student_id,
                   const User users[], int user_count);
Result return_book(Book books[], int book_count, int book_id, int student_id);
int count_borrowed_books(const Book books[], int book_count);
int count_student_loans(const Book books[], int book_count, int student_id);

/* seat.c (류지언) : F4 좌석 예약·취소, F6 시간대 예약 */
Result reserve_seat(Seat seats[], int seat_number, int student_id,
                    const User users[], int user_count);
Result cancel_seat(Seat seats[], int seat_number, int student_id);
Result reserve_time_slot(Seat *seat, int start_hour, int end_hour, int student_id,
                         const User users[], int user_count);
Result cancel_time_slot(Seat *seat, int start_hour, int student_id);
int count_empty_seats(const Seat seats[]);

/* file.c (최민호) : F5 저장·불러오기, 도서 동적 배열 */
Result load_all(LibraryData *data);
Result save_all(const LibraryData *data);
Result add_book(LibraryData *data, const Book *book);
void free_all(LibraryData *data);
Result append_admin_log(const char *event);

/* admin.c (송영준) : F7 관리자 로그인과 관리 기능 */
int is_valid_password(const char *password);
int check_admin_password(const LibraryData *data, const char *input);
Result change_admin_password(LibraryData *data, const char *new_password);
Result login_admin(const LibraryData *data, LoginGuard *guard, const char *input,
                   long long now, int *wait_seconds);
int login_wait_seconds(const LoginGuard *guard, long long now);
Result login_result(LoginGuard *guard, int password_ok, long long now, int *wait_seconds);
Result admin_add_book(LibraryData *data, int book_id, const char *title, const char *author);
Result admin_remove_book(LibraryData *data, int book_id);
Result admin_return_book(LibraryData *data, int book_id);
Result admin_add_user(LibraryData *data, int student_id, char temp_password[TEMP_PASSWORD_LEN]);
Result admin_reset_password(LibraryData *data, int student_id,
                            char temp_password[TEMP_PASSWORD_LEN]);
Result admin_remove_user(LibraryData *data, int student_id);
Result admin_clear_seat(LibraryData *data, int seat_number);
void admin_close_day(LibraryData *data);
int find_user_seat(const LibraryData *data, int student_id);

/* crypto.c (유강훈) : 비밀번호 해시 (SHA-256, PBKDF2) */
void make_salt(char salt_hex[SALT_HEX_LEN]);
void make_temp_password(char password[TEMP_PASSWORD_LEN]);
void hash_password(const char *password, const char *salt_hex, char hash_hex[HASH_HEX_LEN]);
int same_hash(const char *hash_a, const char *hash_b);

#endif
