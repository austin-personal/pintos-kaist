#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// printf("system call!\n");
	char *fn_copy;
	int siz;
	
	switch (f->R.rax)
	{
	case SYS_HALT:
		printf("halt!\n");
		halt();
		break;
	case SYS_EXIT:
		// printf("exit!\n");
		exit(f->R.rdi);
		break;
	case SYS_FORK:
		printf("fork!\n");
		// f->R.rax = fork(f->R.rdi, f);
		break;
	case SYS_EXEC:
		printf("exec!\n");
		// if (exec(f->R.rdi) == -1)
		// 	exit(-1);
		break;
	case SYS_WAIT:
		printf("wait!\n");
		// f->R.rax = process_wait(f->R.rdi);
		break;
	case SYS_CREATE:
		// printf("create!\n");
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		// printf("remove!\n");
		f->R.rax = remove(f->R.rdi);
		break;
	case SYS_OPEN:
		// printf("open!\n");
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		printf("filesize!\n");
		// f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		printf("read!\n");
		// f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		// printf("write!\n");
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		printf("seek!\n");
		// seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		printf("tell!\n");
		// f->R.rax = tell(f->R.rdi);
		break;
	case SYS_CLOSE:
		printf("close!\n");
		// close(f->R.rdi);
		break;
	case SYS_DUP2:
		printf("dup2!\n");
		// f->R.rax = dup2(f->R.rdi, f->R.rsi);
		break;
	default:
		// printf("default exit!\n");
		exit(-1);
		break;
	}
	// thread_exit ();
}
