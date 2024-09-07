#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void sys_halt(void);
void sys_exit(int status);
bool sys_create(const char *file, unsigned initial_size);
int sys_open(const char *file);
int sys_write(int fd, const void *buffer, unsigned size);
/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
{
	struct thread *cur = thread_current();
	cur->is_user = true;
	// printf("system call!\n");
	// 시스템 콜 번호
	int number = f->R.rax;
	// TODO: Your implementation goes here.
	switch (number)
	{
	case SYS_HALT:
	{
		sys_halt();
	}
	break;
	case SYS_EXIT:
	{
		sys_exit(f->R.rdi);
	}
	break;
	case SYS_CREATE:
	{
		// printf("%s %d\n", f->R.rdi, f->R.rsi);
		f->R.rax = sys_create(f->R.rdi, f->R.rsi);
	}
	break;
	case SYS_OPEN:
	{
		f->R.rax = sys_open(f->R.rdi);
	}
	break;
	case SYS_WRITE:
	{
		int fd = f->R.rdi;
		void *buffer = f->R.rsi;
		unsigned size = f->R.rdx;
		f->R.rax = sys_write(fd, buffer, size);
		// printf("%d %s %d\n", fd, buffer, size);
	}
	break;
	default:
		thread_exit();
	}
}
void sys_halt(void)
{
	power_off();
}
void sys_exit(int status)
{
	struct thread *cur = thread_current();
	cur->exit_status = status;

	thread_exit();
}
bool sys_create(const char *file, unsigned initial_size)
{
	struct thread *cur = thread_current();
	if (file == "" || file == NULL || initial_size == 0 || initial_size == NULL)
	{
		// cur->exit_status = -1;
		return false;
	}
	return filesys_create(file, initial_size);
}

int sys_open(const char *file)
{
	struct file *f = filesys_open(file);
}
int sys_write(int fd, const void *buffer, unsigned size)
{
	if (fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}
}