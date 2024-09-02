#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/fixed_point.h"

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
// staic = 자동으로 0 으로 초기화
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops(unsigned loops);
static void busy_wait(int64_t loops);
static void real_time_sleep(int64_t num, int32_t denom);
static bool wake_time_less(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
// 슬립 상태의 스레드들을 관리하기 위한 리스트
// 이 리스트는 wake_time 순으로 정렬되어 유지됨
static struct list sleep_list; // sleep_list 생성

/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt. */
void timer_init(void)
{
	/* 8254 input frequency divided by TIMER_FREQ, rounded to
	   nearest. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb(0x43, 0x34); /* CW: counter 0, LSB then MSB, mode 2, binary. */
	outb(0x40, count & 0xff);
	outb(0x40, count >> 8);

	/* Initialize load_avg to 0 */
	load_avg = 0;
	intr_register_ext(0x20, timer_interrupt, "8254 Timer");
	// sleep_list를 초기화. 이후 슬립 상태의 스레드들을 관리하는 데 사용됨
	list_init(&sleep_list); // sleep 리스트 초기화
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void timer_calibrate(void)
{
	unsigned high_bit, test_bit;

	ASSERT(intr_get_level() == INTR_ON);
	printf("Calibrating timer...  ");

	/* Approximate loops_per_tick as the largest power-of-two
	   still less than one timer tick. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops(loops_per_tick << 1))
	{
		loops_per_tick <<= 1;
		ASSERT(loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops(high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf("%'" PRIu64 " loops/s.\n", (uint64_t)loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks(void)
{
	enum intr_level old_level = intr_disable();
	int64_t t = ticks;
	intr_set_level(old_level);
	barrier();
	return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed(int64_t then)
{
	return timer_ticks() - then;
}

/* Suspends execution for approximately TICKS timer ticks. */
void timer_sleep(int64_t ticks)
{
	// 입력된 ticks가 0 이하면 함수를 즉시 종료
	if (ticks <= 0)
		return;
	// 인터럽트가 활성화되어 있는지 확인(디버깅 목적)
	ASSERT(intr_get_level() == INTR_ON);
	// 인터럽트를 비활성화 하고 이전 인터럽트 상태를 저장
	// intr_disable() -> 이전 인터럽트 상태 반환함
	enum intr_level old_level = intr_disable();
	// 현재 실행 중인 스레드의 포인터를 가져옴
	struct thread *current = thread_current();
	// timer_ticks() =  시스템이 시작된 이후 경과s한 시간을 "틱(tick)" 단위로 반환
	// 현재 시간에 주어진 ticks를 더해 깨어날 시간을 계산
	current->wake_time = timer_ticks() + ticks;
	// sleep_list에 현재 스레드를 wake_time 순으로 정렬하여 삽입
	list_insert_ordered(&sleep_list, &current->elem, wake_time_less, NULL);
	// 현재 스레드를 블록 상태로 전환
	thread_block();
	// 이전 인터럽트 상태로 복원
	intr_set_level(old_level);
}

// wake_time을 기준으로 두 스레드를 비교하는 함수
static bool
wake_time_less(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
	// list_elem을 포함하는 thread 구조체의 포인터를 얻음
	struct thread *thread_a = list_entry(a, struct thread, elem);
	struct thread *thread_b = list_entry(b, struct thread, elem);
	return thread_a->wake_time < thread_b->wake_time;
}
/* Suspends execution for approximately MS milliseconds. */
void timer_msleep(int64_t ms)
{
	real_time_sleep(ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
void timer_usleep(int64_t us)
{
	real_time_sleep(us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
void timer_nsleep(int64_t ns)
{
	real_time_sleep(ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void timer_print_stats(void)
{
	printf("Timer: %" PRId64 " ticks\n", timer_ticks());
}

/* Timer interrupt handler. */
static void
timer_interrupt(struct intr_frame *args UNUSED)
{
	// 시스템의 전체 틱 수를 증가
	// 이는 시스템이 부팅된 이후 경과한 시간을 측정하는 데 사용됨
	ticks++;
	// thread_tick() 함수 호출
	// 이 함수는 현재 실행 중인 스레드의 실행 시간을 업데이트하고,
	// 필요한 경우 스레드 선점(preemption)을 요청함
	// 스레드의 시간 할당량(time slice) 관리와 공정한 CPU 시간 분배를 담당
	thread_tick();
	// 다단계 피드백 큐 스케줄링일 때만 실행
	if (thread_mlfqs)
	{
		mlfqs_increment_recent_cpu();
		if (ticks % TIMER_FREQ == 0)
		{
			mlfqs_calculate_load_avg();
			mlfqs_recalculate_recent_cpu();
			// 디버그용
			// msg("Time: %d, Recalculating recent_cpu for all threads\n", ticks / TIMER_FREQ);
		}
		if (ticks % 4 == 0)
		{
			mlfqs_recalculate_priority();
		}
	}
	// sleep_list에 있는 스레드들을 체크하여 깨워야 할 스레드가 있는지 확인합니다.
	// sleep_list는 일정 시간 동안 잠자고 있는(sleep) 스레드들의 리스트입니다.
	while (!list_empty(&sleep_list))
	{
		struct thread *t = list_entry(list_front(&sleep_list), struct thread, elem);
		// 스레드의 깨워야 하는 시간이 아직 되지 않았다면,
		// 더 이상 진행하지 않고 루프를 빠져나갑니다.
		if (t->wake_time > ticks)
		{
			break;
		}

		// 스레드를 리스트에서 제거하고,
		list_pop_front(&sleep_list);
		// 스레드를 깨웁니다. (상태를 BLOCKED에서 READY로 변경)
		thread_unblock(t);
		// mlfqs 때 추가
		preempt();
	}
}
/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops(unsigned loops)
{
	/* Wait for a timer tick. */
	int64_t start = ticks;
	while (ticks == start)
		barrier();

	/* Run LOOPS loops. */
	start = ticks;
	busy_wait(loops);

	/* If the tick count changed, we iterated too long. */
	barrier();
	return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait(int64_t loops)
{
	while (loops-- > 0)
		barrier();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep(int64_t num, int32_t denom)
{
	/* Convert NUM/DENOM seconds into timer ticks, rounding down.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT(intr_get_level() == INTR_ON);
	if (ticks > 0)
	{
		/* We're waiting for at least one full timer tick.  Use
		   timer_sleep() because it will yield the CPU to other
		   processes. */
		timer_sleep(ticks);
	}
	else
	{
		/* Otherwise, use a busy-wait loop for more accurate
		   sub-tick timing.  We scale the numerator and denominator
		   down by 1000 to avoid the possibility of overflow. */
		ASSERT(denom % 1000 == 0);
		busy_wait(loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}