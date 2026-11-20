/*
 * seat.c  (담당: 류지언)  F4 좌석 예약·취소, F6 시간대별 좌석 예약(선택 기능)
 * ------------------------------------------------------------------
 * F4 규칙
 *   - 등록된 학번만 예약할 수 있다
 *   - 이미 예약된 좌석은 예약할 수 없다
 *   - 한 학번은 좌석을 하나만 예약할 수 있다
 *   - 예약한 학번만 취소할 수 있다
 * F6 규칙
 *   - 운영 시간(9시 ~ 18시) 안에서, 시작 시각이 종료 시각보다 빨라야 한다
 *   - 같은 좌석의 기존 예약과 시간이 겹치면 예약할 수 없다
 *   - 시간대 예약 취소는 예약한 학번이 같을 때만 (관리자 화면에서 사용)
 */
#include "library.h"
#include <stddef.h>   /* NULL */

/*
 * 좌석 번호(1~12)가 올바르면 그 좌석의 주소를, 틀리면 NULL 을 돌려준다.
 * static: 이 파일 안에서만 쓰는 함수라는 뜻 (다른 파일에서는 부를 수 없다)
 */
static Seat *get_seat(Seat seats[], int seat_number)
{
    if (seat_number < 1 || seat_number > SEAT_COUNT) {
        return NULL;
    }
    return &seats[seat_number - 1];   /* 좌석 1번은 배열의 0번 칸 */
}

/* 이 학번이 이미 다른 좌석을 예약했으면 1, 아니면 0 */
static int has_seat(const Seat seats[], int student_id)
{
    int i;

    for (i = 0; i < SEAT_COUNT; i++) {
        if (seats[i].state == SEAT_RESERVED && seats[i].reserver_id == student_id) {
            return 1;
        }
    }
    return 0;
}

/* F4: 좌석을 예약한다 */
Result reserve_seat(Seat seats[], int seat_number, int student_id,
                    const User users[], int user_count)
{
    Seat *seat = get_seat(seats, seat_number);

    if (seat == NULL) {
        return ERR_SEAT_NOT_FOUND;
    }
    if (student_id <= 0) {
        return ERR_INPUT;
    }
    if (!is_registered(users, user_count, student_id)) {
        return ERR_NOT_REGISTERED;
    }
    if (seat->state == SEAT_RESERVED) {
        return ERR_ALREADY_RESERVED;
    }
    if (has_seat(seats, student_id)) {
        return ERR_HAS_SEAT;
    }

    seat->state = SEAT_RESERVED;        /* 포인터(->)로 원본 좌석을 바꾼다 */
    seat->reserver_id = student_id;
    return RESULT_OK;
}

/* F4: 좌석 예약을 취소한다 (예약한 학번만 가능) */
Result cancel_seat(Seat seats[], int seat_number, int student_id)
{
    Seat *seat = get_seat(seats, seat_number);

    if (seat == NULL) {
        return ERR_SEAT_NOT_FOUND;
    }
    if (student_id <= 0) {
        return ERR_INPUT;
    }
    if (seat->state != SEAT_RESERVED) {
        return ERR_NOT_RESERVED;
    }
    if (seat->reserver_id != student_id) {
        return ERR_NOT_RESERVER;
    }

    seat->state = SEAT_EMPTY;
    seat->reserver_id = 0;
    return RESULT_OK;
}

/*
 * 두 시간대가 겹치면 1, 아니면 0
 *   예) 기존 10~12시, 새 11~13시 → 겹침
 *       기존 10~12시, 새 12~14시 → 안 겹침 (끝나는 시각에 바로 시작하는 것은 괜찮다)
 */
static int is_overlap(const TimeSlot *slot, int start_hour, int end_hour)
{
    return start_hour < slot->end_hour && end_hour > slot->start_hour;
}

/*
 * F6: 좌석 하나에 시간대 예약을 추가한다.
 *   seat 는 좌석 한 개의 주소 (예: &data.seats[4] → 5번 좌석)
 */
Result reserve_time_slot(Seat *seat, int start_hour, int end_hour, int student_id,
                         const User users[], int user_count)
{
    int i;
    int pos;

    if (seat == NULL) {
        return ERR_SEAT_NOT_FOUND;
    }
    if (student_id <= 0 || start_hour < OPEN_HOUR || end_hour > CLOSE_HOUR
        || start_hour >= end_hour) {
        return ERR_INPUT;
    }
    if (!is_registered(users, user_count, student_id)) {
        return ERR_NOT_REGISTERED;
    }

    /* 기존 예약과 하나씩 비교해서 겹치는지 확인 */
    for (i = 0; i < seat->slot_count; i++) {
        if (is_overlap(&seat->slots[i], start_hour, end_hour)) {
            return ERR_TIME_OVERLAP;
        }
    }
    if (seat->slot_count >= MAX_TIME_SLOTS) {
        return ERR_TIME_FULL;
    }

    /* 시작 시각 순서로 보이도록, 들어갈 자리를 찾으면서 뒤쪽 예약을 한 칸씩 민다 */
    pos = seat->slot_count;
    while (pos > 0 && seat->slots[pos - 1].start_hour > start_hour) {
        seat->slots[pos] = seat->slots[pos - 1];   /* 구조체는 = 로 통째로 복사된다 */
        pos--;
    }
    seat->slots[pos].start_hour = start_hour;
    seat->slots[pos].end_hour = end_hour;
    seat->slots[pos].student_id = student_id;
    seat->slot_count++;
    return RESULT_OK;
}

/*
 * F6: 시간대 예약 한 건을 취소한다.
 *   start_hour 에 시작하는 예약을 찾고, 예약한 학번(student_id)이 같을 때만 지운다.
 */
Result cancel_time_slot(Seat *seat, int start_hour, int student_id)
{
    int index = -1;
    int i;

    if (seat == NULL) {
        return ERR_SEAT_NOT_FOUND;
    }
    for (i = 0; i < seat->slot_count; i++) {
        if (seat->slots[i].start_hour == start_hour) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        return ERR_NOT_RESERVED;
    }
    if (seat->slots[index].student_id != student_id) {
        return ERR_NOT_RESERVER;
    }

    for (i = index; i < seat->slot_count - 1; i++) {   /* 뒤 예약을 한 칸씩 앞으로 당긴다 */
        seat->slots[i] = seat->slots[i + 1];
    }
    seat->slot_count--;
    return RESULT_OK;
}

/* 비어 있는 좌석 수를 센다 (홈 화면과 좌석 화면에서 사용) */
int count_empty_seats(const Seat seats[])
{
    int i;
    int count = 0;

    for (i = 0; i < SEAT_COUNT; i++) {
        if (seats[i].state == SEAT_EMPTY) {
            count++;
        }
    }
    return count;
}
