#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#define F (1 << 14) // fixed point 1.0 = 2^14 = 16384
// 정수를 고정 소수점으로 변환
#define INT_FP(n) ((n) * F)

// 고정 소수점을 정수로 변환(버림)
#define FP_TO_INT(x) ((x) / F)

// x가 양수일때 내림, x가 음수일 때 올림
#define FP_TO_INT_ROUND(x) (((x) >= 0) ? ((x) + (F / 2)) / F : ((x) - (F / 2)) / F)
// 고정 소수점 더하기 연산
#define ADD_FP(x, y) ((x) + (y))

// 고정 소수점 빼기 연산
#define SUB_FP(x, y) ((x) - (y))
// 고정 소수점  곱셈 연산 (오버플로우 때문에 64비트로 확장)
// x에만 int64_t를 붙인 이유: 연산 시 작은 타입은 더 큰 타입으로 자동 변환
#define MUL_FP(x, y) ((int64_t)(x) * (y) / F)
// 고정 소수점  나눗셈 연산 (오버플로우 때문에 64비트로 확장)
#define DIV_FP(x, y) ((int64_t)(x) * F / (y))

// 고정 소수점과 정수의 덧셈
#define ADD_FP_INT(x, n) ((x) + (n) * F)
// 고정 소수점과 정수의 뺄셈
#define SUB_FP_INT(x, n) ((x) - (n) * F)
// 고정 소수점과 정수의 곱셈
#define MUL_FP_INT(x, n) ((x) * (n))
// 고정 소수점과 정수의 나눗셈
#define DIV_FP_INT(x, n) ((x) / (n))
#endif /* threads/fixed_point.h */