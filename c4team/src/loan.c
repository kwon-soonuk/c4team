/*
 * loan.c  (담당: 유강훈)  F3 도서 대출·반납
 * ------------------------------------------------------------------
 * 대출과 반납의 "규칙"을 검사하고 도서 상태를 바꾼다.
 *   - 등록되지 않은 학번은 대출할 수 없다
 *   - 이미 대출 중인 도서는 대출할 수 없다 (중복 대출 막기)
 *   - 한 학생은 동시에 MAX_LOANS(10)권까지만 빌릴 수 있다
 *   - 대출한 학생만 반납할 수 있다
 */
#include "library.h"

/*
 * 도서를 대출한다.
 *   books 는 배열(주소)로 넘어오기 때문에, 여기서 바꾸면 원본 데이터가 바뀐다.
 *   반환값: RESULT_OK 또는 실패 이유(Result)
 */
Result borrow_book(Book books[], int book_count, int book_id, int student_id,
                   const User users[], int user_count)
{
    int index;
    Book *book;   /* 바꿀 도서를 가리키는 포인터 */

    if (book_id <= 0 || student_id <= 0) {
        return ERR_INPUT;
    }
    if (!is_registered(users, user_count, student_id)) {
        return ERR_NOT_REGISTERED;
    }

    index = find_book_index(books, book_count, book_id);   /* search.c 의 함수 */
    if (index == -1) {
        return ERR_BOOK_NOT_FOUND;
    }

    book = &books[index];
    if (book->state == BOOK_BORROWED) {
        return ERR_ALREADY_BORROWED;
    }
    if (count_student_loans(books, book_count, student_id) >= MAX_LOANS) {
        return ERR_LOAN_LIMIT;
    }

    book->state = BOOK_BORROWED;        /* 포인터로 원본 도서의 상태를 바꾼다 */
    book->borrower_id = student_id;
    return RESULT_OK;
}

/*
 * 도서를 반납한다. 대출한 학번과 반납하는 학번이 같아야 한다.
 */
Result return_book(Book books[], int book_count, int book_id, int student_id)
{
    int index;
    Book *book;

    if (book_id <= 0 || student_id <= 0) {
        return ERR_INPUT;
    }

    index = find_book_index(books, book_count, book_id);
    if (index == -1) {
        return ERR_BOOK_NOT_FOUND;
    }

    book = &books[index];
    if (book->state != BOOK_BORROWED) {
        return ERR_NOT_BORROWED;
    }
    if (book->borrower_id != student_id) {
        return ERR_NOT_BORROWER;
    }

    book->state = BOOK_AVAILABLE;
    book->borrower_id = 0;
    return RESULT_OK;
}

/* 이 학번이 지금 빌리고 있는 도서가 몇 권인지 센다 (권수 제한, 관리자 화면에서 사용) */
int count_student_loans(const Book books[], int book_count, int student_id)
{
    int i;
    int count = 0;

    for (i = 0; i < book_count; i++) {
        if (books[i].state == BOOK_BORROWED && books[i].borrower_id == student_id) {
            count++;
        }
    }
    return count;
}

/* 대출 중인 도서가 몇 권인지 센다 (홈 화면 카드에서 사용) */
int count_borrowed_books(const Book books[], int book_count)
{
    int i;
    int count = 0;

    for (i = 0; i < book_count; i++) {
        if (books[i].state == BOOK_BORROWED) {
            count++;
        }
    }
    return count;
}
