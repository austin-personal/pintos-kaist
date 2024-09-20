#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,	/* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING	/* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) - 1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0	   /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63	   /* Highest priority. */
#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0
/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread
{
	/* Owned by thread.c. */
	tid_t tid; /* Thread identifier. */					// 스레드식별자
	enum thread_status status; /* Thread state. */		// 스레드상태
	char name[16]; /* Name (for debugging purposes). */ // 디버깅목적이름
	int priority;
	int original_priority; /* Priority. */ // 우선순위

	struct lock *wait_on_lock;		// lock들을 받을 리스트
	struct list donations;			// 이 스레드에게 priority를 나누어준 스레드들의 리스트
	struct list_elem donation_elem; // thread 구조체와 구분하여 사용
	/* Shared between thread.c and synch.c. */
	struct list_elem elem; /* List element. */
	/* List element for all threads list. */
	struct list_elem allelem;
	int64_t wake_time; // 깨어날 시간

	int nice;		// advanced scheduler  구현을 위한 nice 변수
	int recent_cpu; // advanced scheduler  구현을 위한 recent_cpu 변수

	struct list child_list;
	struct list_elem child_elem;
	struct semaphore fork_sema; // 할일 끝난 자식
	struct semaphore wait_sema;
	struct semaphore exit_sema;
	struct file *running; // 현재 스레드의 실행중인 파일을 저장
	int exit_status;	  // 프로세스 종료 상태

	struct intr_frame parent_if;
	// int child_exit_status[32]; // 죽은 자식 상태 저장
#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4; /* Page map level 4 */
	bool is_user;
	// process_wait/////////////
	struct thread *parent; // 부모 프로세스 포인터 저장
	///////////////////////////
	struct file *fd_table[32]; // 파일 디스크립터 생성
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;		  /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

extern int load_avg;

extern struct list all_list;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void mlfqs_calculate_priority(struct thread *t);
void mlfqs_calculate_recent_cpu(struct thread *t);
void mlfqs_calculate_load_avg(void);
void mlfqs_increment_recent_cpu(void);
void mlfqs_recalculate_recent_cpu(void);
void mlfqs_recalculate_priority(void);

void do_iret(struct intr_frame *tf);

bool priority_less(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
void preempt(void);

bool priority_less(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
void preempt(void);
bool thread_compare_donate_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
void donate_priority(struct thread *holder, int new_priority);
void remove_with_lock(struct lock *lock);
void refresh_priority(void);

struct thread *get_child_process(tid_t child_tid);

#endif /* threads/thread.h */
