/*
 * admin.c  (담당: 송영준)  F7 관리자 로그인과 관리 기능 (로직)
 * ------------------------------------------------------------------
 * 관리자(조교·근로학생)만 할 수 있는 일의 "규칙"을 검사하고 데이터를 바꾼다.
 *   - 비밀번호 확인과 변경 (비밀번호는 crypto.c 의 해시로만 비교·저장한다)
 *   - 로그인 시도 제한: 5번 연속으로 틀리면 10초 동안 로그인 불가
 *   - 도서 추가·삭제, 관리자 반납(학번 없이 반납 처리)
 *   - 학번 등록(임시 비밀번호 발급)·삭제, 비밀번호 초기화
 *   - 좌석 비우기, 하루 마감(모든 좌석과 시간대 예약 비우기)
 * 로그인했는지 확인하는 일은 화면 코드(page_admin.c, main.c)가 한다.
 */
#include "library.h"
#include <string.h>   /* strlen, strcpy, strchr */
#include <ctype.h>    /* isalnum */

/* ===================== 비밀번호 ===================== */

/* 비밀번호 형식이 맞으면 1: 영문과 숫자만, 4글자 이상 20글자 이하 */
int is_valid_password(const char *password)
{
    int length = (int)strlen(password);
    int i;

    if (length < PASSWORD_MIN || length >= PASSWORD_LEN) {
        return 0;
    }
    for (i = 0; i < length; i++) {
        if (!isalnum((unsigned char)password[i])) {   /* 영문·숫자가 아니면 */
            return 0;
        }
    }
    return 1;
}

/*
 * 입력한 비밀번호가 맞으면 1, 틀리면 0.
 * 저장된 것은 해시뿐이라서, 입력을 같은 솔트로 해시한 뒤 해시끼리 비교한다.
 */
int check_admin_password(const LibraryData *data, const char *input)
{
    char input_hash[HASH_HEX_LEN];

    if (strlen(input) >= PASSWORD_LEN) {   /* 너무 긴 입력은 계산하지 않고 거절 */
        return 0;
    }
    hash_password(input, data->admin_salt, input_hash);   /* crypto.c */
    return same_hash(input_hash, data->admin_hash);
}

/* 관리자 비밀번호를 바꾼다: 새 솔트를 만들고, 비밀번호 대신 해시를 기억한다 */
Result change_admin_password(LibraryData *data, const char *new_password)
{
    if (!is_valid_password(new_password)) {
        return ERR_PASSWORD;
    }
    make_salt(data->admin_salt);
    hash_password(new_password, data->admin_salt, data->admin_hash);
    return RESULT_OK;
}

/* 로그인이 막혀 있으면 남은 시간(초), 아니면 0 */
int login_wait_seconds(const LoginGuard *guard, long long now)
{
    if (guard->locked_until > now) {
        return (int)(guard->locked_until - now);
    }
    return 0;
}

/*
 * 관리자 로그인을 시도한다.
 *   guard        : 실패 횟수와 잠금 상태 (화면 코드가 하나 만들어 계속 넘겨준다)
 *   now          : 지금 시각 (time(NULL) 값). 매개변수로 받아서 시간을 바꿔 가며 테스트할 수 있다
 *   wait_seconds : 잠겨 있으면 남은 시간(초)을 넣어 준다
 *   반환값: RESULT_OK(성공), ERR_LOGIN_FAILED(틀림), ERR_LOGIN_LOCKED(잠김)
 * LOGIN_MAX_FAILS(5)번 이상 연속으로 틀리면, 틀릴 때마다 LOGIN_LOCK_SECONDS(10)초씩 막는다.
 */
Result login_admin(const LibraryData *data, LoginGuard *guard, const char *input,
                   long long now, int *wait_seconds)
{
    *wait_seconds = login_wait_seconds(guard, now);
    if (*wait_seconds > 0) {
        return ERR_LOGIN_LOCKED;   /* 잠긴 동안에는 비밀번호가 맞아도 확인하지 않는다 */
    }

    return login_result(guard, check_admin_password(data, input), now, wait_seconds);
}

/*
 * 비밀번호가 맞았는지(password_ok)에 따라 실패 횟수와 잠금을 고치고 결과를 돌려준다.
 * 관리자 로그인(login_admin)과 학생 로그인(user.c 의 login_student)이 함께 쓴다.
 */
Result login_result(LoginGuard *guard, int password_ok, long long now, int *wait_seconds)
{
    if (password_ok) {
        guard->fail_count = 0;
        guard->locked_until = 0;
        return RESULT_OK;
    }

    guard->fail_count++;
    if (guard->fail_count >= LOGIN_MAX_FAILS) {
        guard->locked_until = now + LOGIN_LOCK_SECONDS;
        *wait_seconds = LOGIN_LOCK_SECONDS;
        return ERR_LOGIN_LOCKED;
    }
    return ERR_LOGIN_FAILED;
}

/* ===================== 도서 관리 ===================== */

/*
 * 제목·저자로 쓸 수 있는 글자인지 확인한다.
 * 비어 있거나, 빈칸으로 시작하거나, 너무 길거나, 쉼표(,)가 있으면 0
 * (쉼표는 books.txt 에서 칸을 나누는 글자라서 들어가면 파일이 깨진다)
 */
static int is_valid_text(const char *text, int max_len)
{
    int length = (int)strlen(text);

    if (length == 0 || length >= max_len || text[0] == ' ') {
        return 0;
    }
    if (strchr(text, ',') != NULL) {
        return 0;
    }
    return 1;
}

/* 새 도서를 추가한다. 배열이 꽉 차면 add_book() 안에서 realloc 으로 늘어난다 */
Result admin_add_book(LibraryData *data, int book_id, const char *title, const char *author)
{
    Book book;

    if (book_id <= 0 || !is_valid_text(title, TITLE_LEN) || !is_valid_text(author, AUTHOR_LEN)) {
        return ERR_INPUT;
    }
    if (find_book_index(data->books, data->book_count, book_id) != -1) {   /* search.c */
        return ERR_DUPLICATE;
    }

    book.id = book_id;
    strcpy(book.title, title);
    strcpy(book.author, author);
    book.state = BOOK_AVAILABLE;
    book.borrower_id = 0;
    return add_book(data, &book);   /* file.c */
}

/* 도서를 삭제한다. 대출 중인 도서는 삭제할 수 없다 */
Result admin_remove_book(LibraryData *data, int book_id)
{
    int index = find_book_index(data->books, data->book_count, book_id);
    int i;

    if (index == -1) {
        return ERR_BOOK_NOT_FOUND;
    }
    if (data->books[index].state == BOOK_BORROWED) {
        return ERR_IN_USE;
    }

    /* 지운 칸 뒤에 있는 도서를 한 칸씩 앞으로 당긴다 */
    for (i = index; i < data->book_count - 1; i++) {
        data->books[i] = data->books[i + 1];
    }
    data->book_count--;
    return RESULT_OK;
}

/* 관리자 반납: 학번 없이 도서 번호만으로 반납 처리한다 (책만 두고 간 경우 등) */
Result admin_return_book(LibraryData *data, int book_id)
{
    int index = find_book_index(data->books, data->book_count, book_id);

    if (index == -1) {
        return ERR_BOOK_NOT_FOUND;
    }
    if (data->books[index].state != BOOK_BORROWED) {
        return ERR_NOT_BORROWED;
    }
    data->books[index].state = BOOK_AVAILABLE;
    data->books[index].borrower_id = 0;
    return RESULT_OK;
}

/* ===================== 학번 관리 ===================== */

/* 이 학번이 예약한 좌석 번호를 돌려준다. 없으면 0 */
int find_user_seat(const LibraryData *data, int student_id)
{
    int i;

    for (i = 0; i < SEAT_COUNT; i++) {
        if (data->seats[i].state == SEAT_RESERVED && data->seats[i].reserver_id == student_id) {
            return data->seats[i].number;
        }
    }
    return 0;
}

/* 이 학번의 시간대 예약이 하나라도 있으면 1 */
static int has_time_slot(const LibraryData *data, int student_id)
{
    int i, j;

    for (i = 0; i < SEAT_COUNT; i++) {
        for (j = 0; j < data->seats[i].slot_count; j++) {
            if (data->seats[i].slots[j].student_id == student_id) {
                return 1;
            }
        }
    }
    return 0;
}

/*
 * 새 학번을 명단에 등록하고 임시 비밀번호를 발급한다. (8자리 숫자만)
 *   temp_password : 발급한 임시 비밀번호를 넣어 준다. 관리자가 학생에게 전달한다.
 *                   프로그램은 해시만 기억하므로, 이때가 임시 비밀번호를 볼 수 있는 유일한 순간이다.
 */
Result admin_add_user(LibraryData *data, int student_id, char temp_password[TEMP_PASSWORD_LEN])
{
    if (student_id < STUDENT_ID_MIN || student_id > STUDENT_ID_MAX) {
        return ERR_INPUT;
    }
    if (is_registered(data->users, data->user_count, student_id)) {   /* user.c */
        return ERR_DUPLICATE;
    }
    if (data->user_count >= MAX_USERS) {
        return ERR_FULL;
    }
    data->users[data->user_count].id = student_id;
    make_temp_password(temp_password);                                    /* crypto.c */
    set_user_password(&data->users[data->user_count], temp_password, 1);  /* user.c : 임시(1) */
    data->user_count++;
    return RESULT_OK;
}

/* 비밀번호를 잊은 학생에게 임시 비밀번호를 다시 발급한다 (원래 비밀번호는 관리자도 알 수 없다) */
Result admin_reset_password(LibraryData *data, int student_id,
                            char temp_password[TEMP_PASSWORD_LEN])
{
    int index = find_user_index(data->users, data->user_count, student_id);   /* user.c */

    if (index == -1) {
        return ERR_NOT_REGISTERED;
    }
    make_temp_password(temp_password);
    set_user_password(&data->users[index], temp_password, 1);
    return RESULT_OK;
}

/* 학번을 명단에서 삭제한다. 대출이나 예약이 남아 있으면 삭제할 수 없다 */
Result admin_remove_user(LibraryData *data, int student_id)
{
    int index = find_user_index(data->users, data->user_count, student_id);   /* user.c */
    int i;

    if (index == -1) {
        return ERR_NOT_REGISTERED;
    }
    if (count_student_loans(data->books, data->book_count, student_id) > 0   /* loan.c */
        || find_user_seat(data, student_id) != 0
        || has_time_slot(data, student_id)) {
        return ERR_IN_USE;
    }

    for (i = index; i < data->user_count - 1; i++) {   /* 뒤 학번을 한 칸씩 앞으로 */
        data->users[i] = data->users[i + 1];
    }
    data->user_count--;
    return RESULT_OK;
}

/* ===================== 좌석 관리 ===================== */

/* 좌석 하나를 비운다 (자리를 비우고 간 학생의 예약을 관리자가 해제) */
Result admin_clear_seat(LibraryData *data, int seat_number)
{
    Seat *seat;

    if (seat_number < 1 || seat_number > SEAT_COUNT) {
        return ERR_SEAT_NOT_FOUND;
    }
    seat = &data->seats[seat_number - 1];
    if (seat->state != SEAT_RESERVED) {
        return ERR_NOT_RESERVED;
    }
    seat->state = SEAT_EMPTY;
    seat->reserver_id = 0;
    return RESULT_OK;
}

/* 하루 마감: 모든 좌석 예약과 시간대 예약을 비운다 (도서 대출은 그대로 둔다) */
void admin_close_day(LibraryData *data)
{
    int i;

    for (i = 0; i < SEAT_COUNT; i++) {
        data->seats[i].state = SEAT_EMPTY;
        data->seats[i].reserver_id = 0;
        data->seats[i].slot_count = 0;
    }
}
