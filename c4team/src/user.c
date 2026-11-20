/*
 * user.c  (담당: 최민호)  F8 학생 계정: 등록 확인, 학생 로그인, 비밀번호 변경 (로직)
 * ------------------------------------------------------------------
 * 학생은 학번과 비밀번호로 로그인해야 대출·반납·좌석 예약을 할 수 있다.
 *   - 계정은 관리자가 등록하고(admin.c), 그때 임시 비밀번호가 발급된다
 *   - 임시 비밀번호로 로그인하면 반드시 새 비밀번호로 바꿔야 한다 (must_change)
 *   - 비밀번호는 관리자 비밀번호와 똑같이 솔트 + 해시로만 저장한다 (crypto.c)
 * 대출(loan.c)과 좌석 예약(seat.c)은 여기의 is_registered() 로 등록 여부를 확인한다.
 */
#include "library.h"
#include <string.h>

/* 학번으로 명단에서의 위치(인덱스)를 찾는다. 없으면 -1 */
int find_user_index(const User users[], int user_count, int student_id)
{
    int i;

    for (i = 0; i < user_count; i++) {
        if (users[i].id == student_id) {
            return i;
        }
    }
    return -1;
}

/* 학번이 명단에 있으면 1, 없으면 0 */
int is_registered(const User users[], int user_count, int student_id)
{
    return find_user_index(users, user_count, student_id) != -1;
}

/*
 * 학생 한 명의 비밀번호를 정한다: 새 솔트를 만들고 해시를 기억한다.
 *   must_change : 1 이면 임시 비밀번호(로그인 후 바꿔야 함), 0 이면 학생이 직접 정한 비밀번호
 */
void set_user_password(User *user, const char *password, int must_change)
{
    make_salt(user->salt);                               /* crypto.c */
    hash_password(password, user->salt, user->hash);     /* crypto.c */
    user->must_change = must_change;
}

/* 입력한 비밀번호가 이 학생의 비밀번호와 같으면 1, 다르면 0 */
int check_user_password(const User *user, const char *input)
{
    char input_hash[HASH_HEX_LEN];

    if (strlen(input) >= PASSWORD_LEN) {
        return 0;
    }
    hash_password(input, user->salt, input_hash);
    return same_hash(input_hash, user->hash);
}

/*
 * 학생 로그인을 시도한다.
 *   반환값: RESULT_OK(성공), ERR_LOGIN_FAILED(틀림), ERR_LOGIN_LOCKED(여러 번 틀려서 잠김)
 * 등록되지 않은 학번과 틀린 비밀번호를 똑같이 ERR_LOGIN_FAILED 로 돌려준다.
 * ("그런 학번은 없습니다"라고 알려 주면, 어떤 학번이 등록되어 있는지 알아낼 수 있기 때문)
 */
Result login_student(const LibraryData *data, LoginGuard *guard, int student_id,
                     const char *input, long long now, int *wait_seconds)
{
    int index;
    int password_ok = 0;

    *wait_seconds = login_wait_seconds(guard, now);   /* admin.c */
    if (*wait_seconds > 0) {
        return ERR_LOGIN_LOCKED;
    }

    index = find_user_index(data->users, data->user_count, student_id);
    if (index != -1) {
        password_ok = check_user_password(&data->users[index], input);
    }
    return login_result(guard, password_ok, now, wait_seconds);   /* admin.c */
}

/* 이 학번이 임시 비밀번호를 쓰고 있으면(바꿔야 하면) 1, 아니면 0 */
int must_change_password(const LibraryData *data, int student_id)
{
    int index = find_user_index(data->users, data->user_count, student_id);

    if (index == -1) {
        return 0;
    }
    return data->users[index].must_change;
}

/*
 * 학생이 자기 비밀번호를 바꾼다. (로그인한 학생만 화면에서 부를 수 있다)
 *   형식이 틀리면 ERR_PASSWORD, 지금 비밀번호와 같으면 ERR_SAME_PASSWORD
 */
Result change_student_password(LibraryData *data, int student_id, const char *new_password)
{
    int index = find_user_index(data->users, data->user_count, student_id);

    if (index == -1) {
        return ERR_NOT_REGISTERED;
    }
    if (!is_valid_password(new_password)) {   /* admin.c : 영문·숫자 4~20글자 */
        return ERR_PASSWORD;
    }
    if (check_user_password(&data->users[index], new_password)) {
        return ERR_SAME_PASSWORD;             /* 임시 비밀번호를 그대로 다시 쓰지 못하게 */
    }
    set_user_password(&data->users[index], new_password, 0);
    return RESULT_OK;
}
