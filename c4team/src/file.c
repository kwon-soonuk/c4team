/*
 * file.c  (담당: 최민호)  F5 저장·불러오기 + 도전 목표(도서 동적 배열)
 * ------------------------------------------------------------------
 * data 폴더의 텍스트 파일 5개를 읽고 쓴다. (UTF-8, 한 줄에 하나, 쉼표로 구분)
 *   books.txt : 도서번호,제목,저자,상태(0 대출 가능 / 1 대출 중),대출자학번
 *   seats.txt : 좌석번호,상태(0 비어 있음 / 1 예약 중),예약자학번
 *   slots.txt : 좌석번호,시작시각,종료시각,학번          (F6 시간대 예약)
 *   users.txt : 학번,임시여부,솔트,해시                  (F8 학생 계정. 비밀번호 자체는 없음)
 *   admin.txt : 해시방식,반복횟수,솔트,해시              (F7 관리자 비밀번호. 비밀번호 자체는 없음)
 * 그리고 관리자 로그인 기록을 admin_log.txt 에 이어서 쓴다.
 *
 * 프로그램을 시작할 때 load_all(), [저장 후 종료]를 누를 때 save_all() 이 불린다.
 * 파일이 없으면 초기 데이터를 넣어서 시작한다.
 */
#include "library.h"
#include <stdio.h>    /* fopen, fgets, fprintf, sscanf */
#include <stdlib.h>   /* realloc, free */
#include <string.h>   /* strcpy, strcmp, strlen, strcspn */
#include <time.h>     /* time, localtime, strftime */
#include <direct.h>   /* _mkdir (폴더 만들기) */

#define BOOKS_FILE "data/books.txt"
#define SEATS_FILE "data/seats.txt"
#define SLOTS_FILE "data/slots.txt"
#define USERS_FILE "data/users.txt"
#define ADMIN_FILE "data/admin.txt"
#define ADMIN_LOG_FILE "data/admin_log.txt"
#define LINE_LEN   256   /* 파일에서 한 줄을 읽을 때 쓰는 버퍼 크기 */

/* ===================== 도서 동적 배열 (도전 목표) ===================== */

/*
 * 도서 한 권을 배열 끝에 추가한다.
 * 배열이 꽉 차 있으면 realloc 으로 크기를 2배로 늘린 뒤 추가한다.
 *   처음 크기 10 → 20 → 40 → 80 ... (도서가 몇 권이든 담을 수 있다)
 */
Result add_book(LibraryData *data, const Book *book)
{
    if (data->book_count == data->book_capacity) {
        int new_capacity;
        Book *bigger;

        if (data->book_capacity == 0) {
            new_capacity = FIRST_CAPACITY;
        } else {
            new_capacity = data->book_capacity * 2;
        }

        bigger = (Book *)realloc(data->books, sizeof(Book) * new_capacity);
        if (bigger == NULL) {
            return ERR_MEMORY;   /* 실패하면 기존 배열(data->books)은 그대로 남아 있다 */
        }
        data->books = bigger;
        data->book_capacity = new_capacity;
    }

    data->books[data->book_count] = *book;   /* 구조체는 = 로 통째로 복사된다 */
    data->book_count++;
    return RESULT_OK;
}

/* 프로그램을 끝낼 때 동적 배열 메모리를 돌려준다 */
void free_all(LibraryData *data)
{
    free(data->books);
    data->books = NULL;
    data->book_count = 0;
    data->book_capacity = 0;
}

/* ===================== 불러오기 ===================== */

/*
 * 메모장으로 저장하면 파일 맨 앞에 눈에 안 보이는 3바이트(BOM)가 붙을 수 있다.
 * 있으면 건너뛴 위치를 돌려준다. (없으면 그대로)
 */
static char *skip_bom(char *line)
{
    if ((unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB
        && (unsigned char)line[2] == 0xBF) {
        return line + 3;
    }
    return line;
}

static Result load_books(LibraryData *data)
{
    FILE *fp = fopen(BOOKS_FILE, "r");
    char line[LINE_LEN];
    Book book;
    int state;

    if (fp == NULL) {
        return ERR_FILE;
    }
    while (fgets(line, LINE_LEN, fp) != NULL) {
        /* %79[^,] : 쉼표(,)가 나오기 전까지 최대 79바이트를 문자열로 읽는다
         * (79 = TITLE_LEN - 1, 39 = AUTHOR_LEN - 1 : 상수를 바꾸면 여기도 바꾸기) */
        if (sscanf(skip_bom(line), "%d,%79[^,],%39[^,],%d,%d",
                   &book.id, book.title, book.author, &state, &book.borrower_id) == 5) {
            if (state == 1) {
                book.state = BOOK_BORROWED;
            } else {
                book.state = BOOK_AVAILABLE;
                book.borrower_id = 0;
            }
            /* 같은 도서 번호가 이미 있으면 그 줄은 건너뛴다 (search.c 의 함수 사용) */
            if (find_book_index(data->books, data->book_count, book.id) != -1) {
                continue;
            }
            if (add_book(data, &book) != RESULT_OK) {
                break;   /* 메모리가 부족하면 여기까지만 읽는다 */
            }
        }
    }
    fclose(fp);
    return RESULT_OK;
}

static Result load_seats(LibraryData *data)
{
    FILE *fp = fopen(SEATS_FILE, "r");
    char line[LINE_LEN];
    int number, state, reserver_id;

    if (fp == NULL) {
        return ERR_FILE;
    }
    while (fgets(line, LINE_LEN, fp) != NULL) {
        if (sscanf(skip_bom(line), "%d,%d,%d", &number, &state, &reserver_id) == 3
            && number >= 1 && number <= SEAT_COUNT && state == 1) {
            data->seats[number - 1].state = SEAT_RESERVED;
            data->seats[number - 1].reserver_id = reserver_id;
        }
    }
    fclose(fp);
    return RESULT_OK;
}

static Result load_slots(LibraryData *data)
{
    FILE *fp = fopen(SLOTS_FILE, "r");
    char line[LINE_LEN];
    int number, start_hour, end_hour, student_id;

    if (fp == NULL) {
        return ERR_FILE;
    }
    while (fgets(line, LINE_LEN, fp) != NULL) {
        if (sscanf(skip_bom(line), "%d,%d,%d,%d", &number, &start_hour, &end_hour,
                   &student_id) == 4 && number >= 1 && number <= SEAT_COUNT
            && start_hour >= OPEN_HOUR && end_hour <= CLOSE_HOUR && start_hour < end_hour) {
            Seat *seat = &data->seats[number - 1];

            if (seat->slot_count < MAX_TIME_SLOTS) {
                seat->slots[seat->slot_count].start_hour = start_hour;
                seat->slots[seat->slot_count].end_hour = end_hour;
                seat->slots[seat->slot_count].student_id = student_id;
                seat->slot_count++;
            }
        }
    }
    fclose(fp);
    return RESULT_OK;
}

/* 학번 뒤 4자리로 처음 임시 비밀번호를 만든다. 예) 20261234 → "temp1234" */
static void default_temp_password(int student_id, char password[TEMP_PASSWORD_LEN])
{
    int last4 = student_id % 10000;   /* 뒤 4자리 */

    strcpy(password, "temp");
    password[4] = (char)('0' + last4 / 1000);        /* 숫자 → 글자: '0' 에 더한다 */
    password[5] = (char)('0' + last4 / 100 % 10);
    password[6] = (char)('0' + last4 / 10 % 10);
    password[7] = (char)('0' + last4 % 10);
    password[8] = '\0';
}

/*
 * 학생 계정 명단을 불러온다. users.txt 한 줄의 형식:
 *   학번,임시여부(1 임시 / 0 직접 정함),솔트(16진수 32글자),해시(16진수 64글자)
 * 손으로 계정을 넣을 때를 위해 아래 두 가지도 받아 준다. (다음에 저장하면 위 형식으로 바뀐다)
 *   학번,비밀번호  → 그 비밀번호를 임시 비밀번호로 등록
 *   학번           → "temp" + 학번 뒤 4자리를 임시 비밀번호로 등록
 */
static Result load_users(LibraryData *data)
{
    FILE *fp = fopen(USERS_FILE, "r");
    char line[LINE_LEN];
    char salt[SALT_HEX_LEN];
    char hash[HASH_HEX_LEN];
    char password[PASSWORD_LEN];
    char temp_password[TEMP_PASSWORD_LEN];
    char *text;
    int student_id = 0;
    int must_change = 0;

    if (fp == NULL) {
        return ERR_FILE;
    }
    data->user_count = 0;
    while (data->user_count < MAX_USERS && fgets(line, LINE_LEN, fp) != NULL) {
        User *user = &data->users[data->user_count];

        text = skip_bom(line);
        if (sscanf(text, "%d,%d,%32[0-9a-f],%64[0-9a-f]", &student_id, &must_change, salt, hash) == 4
            && strlen(salt) == SALT_HEX_LEN - 1 && strlen(hash) == HASH_HEX_LEN - 1) {
            strcpy(user->salt, salt);
            strcpy(user->hash, hash);
            user->must_change = (must_change != 0);
        } else if (sscanf(text, "%d,%20s", &student_id, password) == 2
                   && is_valid_password(password)) {
            set_user_password(user, password, 1);                 /* user.c */
        } else if (sscanf(text, "%d", &student_id) == 1 && student_id > 0) {
            default_temp_password(student_id, temp_password);
            set_user_password(user, temp_password, 1);
        } else {
            continue;   /* 형식이 틀린 줄은 건너뛴다 */
        }
        if (student_id <= 0 || is_registered(data->users, data->user_count, student_id)) {
            continue;   /* 잘못된 학번, 겹치는 학번은 건너뛴다 */
        }
        user->id = student_id;
        data->user_count++;
    }
    fclose(fp);
    return RESULT_OK;
}

/*
 * 관리자 비밀번호 정보를 불러온다. admin.txt 한 줄의 형식:
 *   pbkdf2-sha256,반복횟수,솔트(16진수 32글자),해시(16진수 64글자)
 * 비밀번호 자체는 파일에 없고, 솔트와 해시만 있다. (계산은 crypto.c)
 *
 * 그 밖의 경우
 *   - 파일이 없거나 형식이 틀림 → 처음 비밀번호(DEFAULT_PASSWORD)로 정한다
 *   - 한 줄에 비밀번호만 적혀 있음(예전 형식, 또는 비밀번호를 잊어서 직접 적은 경우)
 *     → 그 비밀번호로 정한다. 다음에 저장할 때 해시 형식으로 바뀐다
 */
static void load_admin(LibraryData *data)
{
    FILE *fp = fopen(ADMIN_FILE, "r");
    char line[LINE_LEN];
    char name[20];
    char salt[SALT_HEX_LEN];
    char hash[HASH_HEX_LEN];
    char *text;
    int iterations;
    int loaded = 0;   /* 파일에서 제대로 불러왔으면 1 */

    if (fp != NULL && fgets(line, LINE_LEN, fp) != NULL) {
        text = skip_bom(line);
        text[strcspn(text, "\r\n")] = '\0';   /* 줄 끝의 줄바꿈 글자를 지운다 */

        /* %32[0-9a-f] : 16진수 글자(0~9, a~f)만 최대 32글자 읽는다 */
        if (sscanf(text, "%19[^,],%d,%32[0-9a-f],%64[0-9a-f]", name, &iterations, salt, hash) == 4
            && strcmp(name, HASH_NAME) == 0 && iterations == HASH_ITERATIONS
            && strlen(salt) == SALT_HEX_LEN - 1 && strlen(hash) == HASH_HEX_LEN - 1) {
            strcpy(data->admin_salt, salt);
            strcpy(data->admin_hash, hash);
            loaded = 1;
        } else if (is_valid_password(text)) {      /* 비밀번호만 적혀 있는 경우 */
            change_admin_password(data, text);     /* admin.c : 새 솔트로 해시를 만든다 */
            loaded = 1;
        }
    }
    if (fp != NULL) {
        fclose(fp);
    }
    if (!loaded) {
        change_admin_password(data, DEFAULT_PASSWORD);
    }
}

/* ===================== 초기 데이터 (파일이 없을 때) ===================== */

/* 좌석 12개를 "비어 있음" 상태로 만든다 */
static void init_seats(LibraryData *data)
{
    int i;

    for (i = 0; i < SEAT_COUNT; i++) {
        data->seats[i].number = i + 1;
        data->seats[i].state = SEAT_EMPTY;
        data->seats[i].reserver_id = 0;
        data->seats[i].slot_count = 0;
    }
}

static void set_default_books(LibraryData *data)
{
    Book defaults[] = {
        {101, "C언어 기초", "김코딩", BOOK_AVAILABLE, 0},
        {102, "C언어 실습", "이프로", BOOK_AVAILABLE, 0},
        {103, "자료구조 입문", "박자료", BOOK_AVAILABLE, 0},
        {104, "파이썬 첫걸음", "최파이", BOOK_AVAILABLE, 0},
        {105, "컴퓨터 구조", "정회로", BOOK_AVAILABLE, 0}
    };
    int count = (int)(sizeof(defaults) / sizeof(defaults[0]));   /* 배열 칸 수 */
    int i;

    for (i = 0; i < count; i++) {
        add_book(data, &defaults[i]);
    }
}

static void set_default_users(LibraryData *data)
{
    int defaults[] = {20261234, 20261235, 20265678, 20251001, 20251002};
    int count = (int)(sizeof(defaults) / sizeof(defaults[0]));
    char temp_password[TEMP_PASSWORD_LEN];
    int i;

    data->user_count = 0;
    for (i = 0; i < count; i++) {
        data->users[i].id = defaults[i];
        default_temp_password(defaults[i], temp_password);   /* 예) temp1234 */
        set_user_password(&data->users[i], temp_password, 1);
        data->user_count++;
    }
}

/*
 * 모든 데이터를 파일에서 불러온다. (프로그램 시작 시 한 번)
 *   반환값: RESULT_OK → 도서·명단 파일을 모두 불러옴
 *           ERR_FILE  → 파일이 없어서 일부를 초기 데이터로 채움
 */
Result load_all(LibraryData *data)
{
    Result result = RESULT_OK;

    init_seats(data);

    if (load_books(data) != RESULT_OK) {
        set_default_books(data);
        result = ERR_FILE;
    }
    if (load_users(data) != RESULT_OK) {
        set_default_users(data);
        result = ERR_FILE;
    }
    load_seats(data);   /* 파일이 없으면 모든 좌석이 빈 상태로 시작 */
    load_slots(data);   /* 파일이 없으면 시간대 예약 없이 시작 */
    load_admin(data);   /* 파일이 없으면 처음 비밀번호로 시작 */
    return result;
}

/* ===================== 저장 ===================== */

static Result save_books(const LibraryData *data)
{
    FILE *fp = fopen(BOOKS_FILE, "w");
    int i;

    if (fp == NULL) {
        return ERR_FILE;
    }
    for (i = 0; i < data->book_count; i++) {
        const Book *book = &data->books[i];

        fprintf(fp, "%d,%s,%s,%d,%d\n", book->id, book->title, book->author,
                (int)book->state, book->borrower_id);
    }
    if (fclose(fp) != 0) {   /* 닫을 때 실패하면 제대로 저장되지 않은 것 */
        return ERR_FILE;
    }
    return RESULT_OK;
}

static Result save_seats(const LibraryData *data)
{
    FILE *fp = fopen(SEATS_FILE, "w");
    int i;

    if (fp == NULL) {
        return ERR_FILE;
    }
    for (i = 0; i < SEAT_COUNT; i++) {
        fprintf(fp, "%d,%d,%d\n", data->seats[i].number, (int)data->seats[i].state,
                data->seats[i].reserver_id);
    }
    if (fclose(fp) != 0) {
        return ERR_FILE;
    }
    return RESULT_OK;
}

static Result save_slots(const LibraryData *data)
{
    FILE *fp = fopen(SLOTS_FILE, "w");
    int i, j;

    if (fp == NULL) {
        return ERR_FILE;
    }
    for (i = 0; i < SEAT_COUNT; i++) {
        const Seat *seat = &data->seats[i];

        for (j = 0; j < seat->slot_count; j++) {
            fprintf(fp, "%d,%d,%d,%d\n", seat->number, seat->slots[j].start_hour,
                    seat->slots[j].end_hour, seat->slots[j].student_id);
        }
    }
    if (fclose(fp) != 0) {
        return ERR_FILE;
    }
    return RESULT_OK;
}

static Result save_users(const LibraryData *data)
{
    FILE *fp = fopen(USERS_FILE, "w");
    int i;

    if (fp == NULL) {
        return ERR_FILE;
    }
    for (i = 0; i < data->user_count; i++) {
        fprintf(fp, "%d,%d,%s,%s\n", data->users[i].id, data->users[i].must_change,
                data->users[i].salt, data->users[i].hash);
    }
    if (fclose(fp) != 0) {
        return ERR_FILE;
    }
    return RESULT_OK;
}

static Result save_admin(const LibraryData *data)
{
    FILE *fp = fopen(ADMIN_FILE, "w");

    if (fp == NULL) {
        return ERR_FILE;
    }
    fprintf(fp, "%s,%d,%s,%s\n", HASH_NAME, HASH_ITERATIONS, data->admin_salt, data->admin_hash);
    if (fclose(fp) != 0) {
        return ERR_FILE;
    }
    return RESULT_OK;
}

/*
 * 관리자 기록 파일(admin_log.txt) 맨 뒤에 "날짜 시각,내용" 한 줄을 덧붙인다.
 * 로그인 성공·실패·잠김, 로그아웃이 일어난 바로 그때 기록한다. ("a" = 이어 쓰기 모드)
 * 이 기록은 화면에는 보이지 않고 파일에만 남는다.
 */
Result append_admin_log(const char *event)
{
    FILE *fp;
    time_t now = time(NULL);
    struct tm *local = localtime(&now);   /* 초 단위 시각 → 연, 월, 일, 시, 분, 초 */
    char when[32];

    if (local == NULL) {
        return ERR_FILE;
    }
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", local);

    _mkdir("data");
    fp = fopen(ADMIN_LOG_FILE, "a");
    if (fp == NULL) {
        return ERR_FILE;
    }
    fprintf(fp, "%s,%s\n", when, event);
    if (fclose(fp) != 0) {
        return ERR_FILE;
    }
    return RESULT_OK;
}

/*
 * 모든 데이터를 파일에 저장한다. (관리자의 [지금 저장], [저장 후 종료] 버튼)
 * 하나라도 실패하면 ERR_FILE 을 돌려준다. → 화면에서 오류 안내 후 홈으로 돌아간다
 */
Result save_all(const LibraryData *data)
{
    _mkdir("data");   /* data 폴더가 없으면 만든다 (이미 있으면 아무 일도 하지 않는다) */

    if (save_books(data) != RESULT_OK) {
        return ERR_FILE;
    }
    if (save_seats(data) != RESULT_OK) {
        return ERR_FILE;
    }
    if (save_slots(data) != RESULT_OK) {
        return ERR_FILE;
    }
    if (save_users(data) != RESULT_OK) {
        return ERR_FILE;
    }
    if (save_admin(data) != RESULT_OK) {
        return ERR_FILE;
    }
    return RESULT_OK;
}
