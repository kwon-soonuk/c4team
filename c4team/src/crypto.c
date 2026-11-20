/*
 * crypto.c  (담당: 유강훈)  관리자 비밀번호를 안전하게 저장하기 위한 해시 계산
 * ------------------------------------------------------------------
 * 비밀번호를 파일에 그대로 쓰면 파일을 연 사람 누구나 읽을 수 있다.
 * 그래서 비밀번호 대신 "해시 값"을 저장한다.
 *   - 해시: 글자를 넣으면 64글자짜리 값이 나오는 계산. 같은 입력은 항상 같은 값이 나오지만,
 *           해시 값을 보고 원래 글자를 되돌려 알아낼 수는 없다. (이 파일은 SHA-256 을 쓴다)
 *   - 솔트(salt): 비밀번호마다 붙이는 무작위 글자. 같은 비밀번호라도 저장되는 값이 달라져서,
 *                 미리 만들어 둔 "흔한 비밀번호의 해시 표"로 찾아낼 수 없게 한다.
 *   - 반복: 해시 계산을 20,000번 되풀이해서(PBKDF2), 비밀번호를 하나씩 넣어 보는 공격을 느리게 한다.
 * 로그인할 때는 입력한 비밀번호를 같은 솔트로 다시 계산해서 저장된 해시와 같은지 비교한다.
 *
 * 이 파일은 팀이 만든 알고리즘이 아니라 공개 표준(SHA-256: FIPS 180-4, HMAC: RFC 2104,
 * PBKDF2: RFC 8018)을 그대로 옮긴 것이다. 수업 범위를 넘는 내용이라,
 * "왜 해시로 저장하는지"와 "솔트·반복이 무엇을 막는지"를 설명할 수 있으면 충분하다.
 */
#define _CRT_RAND_S   /* Windows 의 안전한 난수 함수 rand_s 를 쓰기 위해 stdlib.h 보다 먼저 정의 */
#include "library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SHA_BLOCK   64   /* SHA-256 은 64바이트씩 잘라서 계산한다 */
#define SHA_OUTPUT  32   /* 결과는 32바이트(= 16진수 64글자) */

/* ===================== SHA-256 ===================== */

typedef struct {
    unsigned int state[8];              /* 계산 중인 값 8개 (32비트씩) */
    unsigned char buffer[SHA_BLOCK];    /* 아직 64바이트가 차지 않은 입력 */
    int buffer_len;                     /* buffer 에 들어 있는 바이트 수 */
    unsigned long long total_len;       /* 지금까지 넣은 전체 바이트 수 */
} Sha256;

/* 표준에 정해져 있는 상수 64개 */
static const unsigned int SHA_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* 32비트 값을 오른쪽으로 n비트 회전 */
static unsigned int rotate_right(unsigned int value, int n)
{
    return (value >> n) | (value << (32 - n));
}

/* 64바이트 한 덩어리를 계산에 섞는다 (SHA-256 의 핵심 부분) */
static void sha256_block(Sha256 *sha, const unsigned char block[SHA_BLOCK])
{
    unsigned int w[64];
    unsigned int a, b, c, d, e, f, g, h;
    int i;

    for (i = 0; i < 16; i++) {   /* 4바이트씩 묶어 32비트 숫자 16개로 */
        w[i] = ((unsigned int)block[i * 4] << 24) | ((unsigned int)block[i * 4 + 1] << 16)
             | ((unsigned int)block[i * 4 + 2] << 8) | (unsigned int)block[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {  /* 나머지 48개는 앞의 값들을 섞어서 만든다 */
        unsigned int s0 = rotate_right(w[i - 15], 7) ^ rotate_right(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned int s1 = rotate_right(w[i - 2], 17) ^ rotate_right(w[i - 2], 19) ^ (w[i - 2] >> 10);

        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = sha->state[0]; b = sha->state[1]; c = sha->state[2]; d = sha->state[3];
    e = sha->state[4]; f = sha->state[5]; g = sha->state[6]; h = sha->state[7];

    for (i = 0; i < 64; i++) {   /* 64번 섞기 */
        unsigned int s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        unsigned int choose = (e & f) ^ (~e & g);
        unsigned int temp1 = h + s1 + choose + SHA_K[i] + w[i];
        unsigned int s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        unsigned int majority = (a & b) ^ (a & c) ^ (b & c);
        unsigned int temp2 = s0 + majority;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    sha->state[0] += a; sha->state[1] += b; sha->state[2] += c; sha->state[3] += d;
    sha->state[4] += e; sha->state[5] += f; sha->state[6] += g; sha->state[7] += h;
}

static void sha256_start(Sha256 *sha)
{
    /* 표준에 정해져 있는 시작 값 8개 */
    sha->state[0] = 0x6a09e667; sha->state[1] = 0xbb67ae85;
    sha->state[2] = 0x3c6ef372; sha->state[3] = 0xa54ff53a;
    sha->state[4] = 0x510e527f; sha->state[5] = 0x9b05688c;
    sha->state[6] = 0x1f83d9ab; sha->state[7] = 0x5be0cd19;
    sha->buffer_len = 0;
    sha->total_len = 0;
}

/* 입력을 이어 붙인다. 64바이트가 찰 때마다 sha256_block 으로 계산한다 */
static void sha256_add(Sha256 *sha, const unsigned char *data, int length)
{
    int i;

    for (i = 0; i < length; i++) {
        sha->buffer[sha->buffer_len] = data[i];
        sha->buffer_len++;
        if (sha->buffer_len == SHA_BLOCK) {
            sha256_block(sha, sha->buffer);
            sha->buffer_len = 0;
        }
    }
    sha->total_len += (unsigned long long)length;
}

/* 마무리: 표준대로 끝에 0x80, 0 들, 전체 길이(비트)를 붙이고 결과 32바이트를 꺼낸다 */
static void sha256_finish(Sha256 *sha, unsigned char output[SHA_OUTPUT])
{
    unsigned long long bit_len = sha->total_len * 8;
    unsigned char one = 0x80;
    unsigned char zero = 0x00;
    unsigned char length_bytes[8];
    int i;

    sha256_add(sha, &one, 1);
    while (sha->buffer_len != 56) {
        sha256_add(sha, &zero, 1);
    }
    for (i = 0; i < 8; i++) {
        length_bytes[i] = (unsigned char)(bit_len >> (56 - i * 8));
    }
    sha256_add(sha, length_bytes, 8);

    for (i = 0; i < 8; i++) {
        output[i * 4] = (unsigned char)(sha->state[i] >> 24);
        output[i * 4 + 1] = (unsigned char)(sha->state[i] >> 16);
        output[i * 4 + 2] = (unsigned char)(sha->state[i] >> 8);
        output[i * 4 + 3] = (unsigned char)(sha->state[i]);
    }
}

/* ===================== HMAC-SHA256 =====================
 * 열쇠(key)와 내용(data)을 함께 섞어 해시를 만드는 표준 방법 */

static void hmac_sha256(const unsigned char *key, int key_len,
                        const unsigned char *data, int data_len,
                        unsigned char output[SHA_OUTPUT])
{
    unsigned char key_block[SHA_BLOCK];
    unsigned char inner_pad[SHA_BLOCK];
    unsigned char outer_pad[SHA_BLOCK];
    unsigned char inner_hash[SHA_OUTPUT];
    Sha256 sha;
    int i;

    /* 열쇠를 64바이트로 맞춘다 (길면 해시해서 줄이고, 짧으면 뒤를 0으로 채운다) */
    memset(key_block, 0, SHA_BLOCK);
    if (key_len > SHA_BLOCK) {
        sha256_start(&sha);
        sha256_add(&sha, key, key_len);
        sha256_finish(&sha, key_block);
    } else {
        memcpy(key_block, key, (size_t)key_len);
    }
    for (i = 0; i < SHA_BLOCK; i++) {
        inner_pad[i] = key_block[i] ^ 0x36;
        outer_pad[i] = key_block[i] ^ 0x5c;
    }

    sha256_start(&sha);                 /* 안쪽: 해시(안쪽 열쇠 + 내용) */
    sha256_add(&sha, inner_pad, SHA_BLOCK);
    sha256_add(&sha, data, data_len);
    sha256_finish(&sha, inner_hash);

    sha256_start(&sha);                 /* 바깥쪽: 해시(바깥 열쇠 + 안쪽 결과) */
    sha256_add(&sha, outer_pad, SHA_BLOCK);
    sha256_add(&sha, inner_hash, SHA_OUTPUT);
    sha256_finish(&sha, output);
}

/* ===================== 비밀번호 해시 (PBKDF2-HMAC-SHA256) ===================== */

/* 32바이트를 16진수 64글자 문자열로 바꾼다 */
static void to_hex(const unsigned char *bytes, int length, char *hex)
{
    const char *digits = "0123456789abcdef";
    int i;

    for (i = 0; i < length; i++) {
        hex[i * 2] = digits[bytes[i] >> 4];
        hex[i * 2 + 1] = digits[bytes[i] & 0x0f];
    }
    hex[length * 2] = '\0';
}

/*
 * 비밀번호와 솔트로 해시(16진수 64글자)를 만든다.
 *   password : 비밀번호 문자열
 *   salt_hex : 솔트 문자열 (make_salt 로 만든 32글자)
 *   hash_hex : 결과를 넣을 배열 (HASH_HEX_LEN 칸)
 * HMAC 을 HASH_ITERATIONS 번 되풀이하면서 결과를 계속 XOR 로 섞는다.
 */
void hash_password(const char *password, const char *salt_hex, char hash_hex[HASH_HEX_LEN])
{
    unsigned char first_input[SALT_HEX_LEN + 4];
    unsigned char u[SHA_OUTPUT];
    unsigned char result[SHA_OUTPUT];
    int password_len = (int)strlen(password);
    int salt_len = (int)strlen(salt_hex);
    int i, j;

    if (salt_len >= SALT_HEX_LEN) {
        salt_len = SALT_HEX_LEN - 1;
    }

    /* 첫 번째: HMAC(비밀번호, 솔트 + 블록 번호 1) */
    memcpy(first_input, salt_hex, (size_t)salt_len);
    first_input[salt_len] = 0;
    first_input[salt_len + 1] = 0;
    first_input[salt_len + 2] = 0;
    first_input[salt_len + 3] = 1;
    hmac_sha256((const unsigned char *)password, password_len, first_input, salt_len + 4, u);
    memcpy(result, u, SHA_OUTPUT);

    /* 두 번째부터: HMAC(비밀번호, 바로 앞 결과) 를 되풀이하며 XOR */
    for (i = 1; i < HASH_ITERATIONS; i++) {
        hmac_sha256((const unsigned char *)password, password_len, u, SHA_OUTPUT, u);
        for (j = 0; j < SHA_OUTPUT; j++) {
            result[j] ^= u[j];
        }
    }
    to_hex(result, SHA_OUTPUT, hash_hex);
}

/* bytes 배열을 예측할 수 없는 무작위 값 count 개로 채운다 */
static void fill_random(unsigned char *bytes, int count)
{
    int filled = 0;
    int i;

#ifdef _WIN32
    for (i = 0; i < count; i++) {        /* Windows: 운영체제가 주는 안전한 난수 */
        unsigned int number;

        if (rand_s(&number) != 0) {
            break;
        }
        bytes[i] = (unsigned char)number;
        filled++;
    }
#else
    {                                     /* 그 밖의 환경(테스트용): /dev/urandom */
        FILE *fp = fopen("/dev/urandom", "rb");

        if (fp != NULL) {
            filled = (int)fread(bytes, 1, (size_t)count, fp);
            fclose(fp);
        }
    }
#endif
    if (filled < count) {                 /* 위 방법이 실패했을 때만: 시각으로 만든 난수 */
        srand((unsigned int)time(NULL));
        for (i = filled; i < count; i++) {
            bytes[i] = (unsigned char)(rand() % 256);
        }
    }
}

/* 무작위 솔트(16진수 32글자)를 만든다. 비밀번호를 정할 때마다 새로 만든다 */
void make_salt(char salt_hex[SALT_HEX_LEN])
{
    unsigned char bytes[(SALT_HEX_LEN - 1) / 2];

    fill_random(bytes, (SALT_HEX_LEN - 1) / 2);
    to_hex(bytes, (SALT_HEX_LEN - 1) / 2, salt_hex);
}

/*
 * 무작위 임시 비밀번호 8글자를 만든다. (관리자가 학생을 등록하거나 비밀번호를 초기화할 때)
 * 헷갈리기 쉬운 글자(0, o, 1, l, i)는 빼고 32가지 글자만 쓴다.
 */
void make_temp_password(char password[TEMP_PASSWORD_LEN])
{
    const char *letters = "abcdefghjkmnpqrstuvwxyz23456789ab";   /* 앞 32글자만 사용 */
    unsigned char bytes[TEMP_PASSWORD_LEN - 1];
    int i;

    fill_random(bytes, TEMP_PASSWORD_LEN - 1);
    for (i = 0; i < TEMP_PASSWORD_LEN - 1; i++) {
        password[i] = letters[bytes[i] % 32];   /* 256 은 32 로 나누어떨어져서 글자마다 확률이 같다 */
    }
    password[TEMP_PASSWORD_LEN - 1] = '\0';
}

/*
 * 두 해시 문자열이 같으면 1, 다르면 0.
 * strcmp 는 처음 다른 글자에서 바로 멈춰서, 걸린 시간으로 "몇 글자까지 맞았는지" 새어 나갈 수 있다.
 * 그래서 끝까지 모두 비교한 뒤에 결과를 돌려준다.
 */
int same_hash(const char *hash_a, const char *hash_b)
{
    size_t length = strlen(hash_a);
    unsigned char difference = 0;
    size_t i;

    if (length != strlen(hash_b)) {
        return 0;
    }
    for (i = 0; i < length; i++) {
        difference |= (unsigned char)(hash_a[i] ^ hash_b[i]);
    }
    return difference == 0;
}
