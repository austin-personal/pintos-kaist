#include "userprog/syscall.h"
#include <string.h>
#include <stdio.h>
#include <syscall-nr.h>
#include <user/syscall.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "vm/vm.h"
void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void check_ptr(const void *ptr);
void sys_halt(void);
void sys_exit(int status);
bool sys_create(const char *file, unsigned initial_size);
bool sys_remove(const char *file);
int sys_open(const char *file);
int sys_close(int fd);
int sys_read(int fd, void *buffer, unsigned size);
int sys_write(int fd, const void *buffer, unsigned size);
int sys_filesize(int fd);
pid_t sys_fork(const char *thread_name, struct intr_frame *f);
int sys_wait(pid_t pid);
int sys_exec(const char *cmd_line);
void sys_seek(int fd, unsigned position);
unsigned sys_tell(int fd);
void *sys_mmap(void *addr, size_t length, int writable, int fd, off_t offset);
void sys_munmap(void *addr);
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
	cur->rsp = f->rsp;
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
		f->R.rax = sys_create(f->R.rdi, f->R.rsi);
	}
	break;
	case SYS_REMOVE:
	{

		f->R.rax = sys_remove(f->R.rdi);
	}
	break;
	case SYS_OPEN:
	{
		f->R.rax = sys_open(f->R.rdi);
		// printf("open : %d", f->R.rax);
	}
	break;
	case SYS_CLOSE:
	{

		sys_close(f->R.rdi);
	}
	break;
	case SYS_READ:
	{

		int fd = f->R.rdi;
		void *buffer = f->R.rsi;
		unsigned size = f->R.rdx;
		f->R.rax = sys_read(fd, buffer, size);
	}
	break;
	case SYS_WRITE:
	{
		int fd = f->R.rdi;
		void *buffer = f->R.rsi;
		unsigned size = f->R.rdx;
		f->R.rax = sys_write(fd, buffer, size);
	}
	break;
	case SYS_FILESIZE:
	{
		f->R.rax = sys_filesize(f->R.rdi);
	}
	break;
	case SYS_FORK:
	{
		f->R.rax = sys_fork(f->R.rdi, f);
	}
	break;
	case SYS_WAIT:
	{
		f->R.rax = sys_wait(f->R.rdi);
	}
	break;
	case SYS_EXEC:
	{

		if (sys_exec(f->R.rdi) == -1)
		{
			sys_exit(-1);
		}
	}
	break;
	case SYS_SEEK:
	{
		sys_seek(f->R.rdi, f->R.rsi);
	}
	break;
		break;
	case SYS_TELL:
	{
		f->R.rax = sys_tell(f->R.rdi);
	}
	break;
	case SYS_MMAP:
	{
		// printf("addr :%p length:%d writable:%d fd : %d ofs: %d\n", f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		f->R.rax = sys_mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		// printf("mmap: %p\n", f->R.rax);
	}
	break;
	case SYS_MUNMAP:
	{
		// printf("munmap: %p\n", f->R.rdi);
		sys_munmap(f->R.rdi);
	}
	break;
	default:
		thread_exit();
	}
}
// 유효포인터 체크 함수
void check_ptr(const void *ptr)
{
	struct thread *cur = thread_current();
	// multi-oom
	if (ptr > USER_STACK || ptr == NULL)
	{
		sys_exit(-1);
	}
	// 새로운 vm 방식에서는 페이지 폴트를 일으켜야한다.
	// 밑에 방식으로 하면 당연히 NULL이 나오는 상황이라 페이지 폴트 자체가 일어나기도
	// 전에 그냥 exit 해버린다. ->spt테이블안에 페이지가 있음에도 페이지를 가져오지 못함
	// if (pml4_get_page(cur->pml4, ptr) == NULL)
	// {
	// 	sys_exit(-1);
	// }
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
	check_ptr(file);
	if (!is_user_vaddr(file))
	{
		return false;
	}
	if (file == "" || file == NULL)
	{

		return false;
	}
	return filesys_create(file, initial_size);
}

bool sys_remove(const char *file)
{
	return filesys_remove(file);
}

int sys_open(const char *file)
{
	check_ptr(file);
	struct file *f = filesys_open(file);
	if (f == NULL)
	{
		return -1;
	}
	else
	{
		struct thread *t = thread_current();
		int fd;
		for (fd = 3; fd < 32; fd++)
		{
			if (t->fd_table[fd] == NULL)
			{

				t->fd_table[fd] = f;
				return fd;
			}
		}
		file_close(f);
		return -1;
	}
}

int sys_close(int fd)
{
	// printf("close\n");
	struct thread *t = thread_current();
	if (!is_user_vaddr(fd) || fd >= 32 || fd < 0)
	{
		return;
	}
	if (t->fd_table[fd] != NULL)
	{
		file_close(t->fd_table[fd]);
		t->fd_table[fd] = NULL;
	}
}

int sys_read(int fd, void *buffer, unsigned size)
{
	// writecode2 통과 코드
	struct page *find_page = spt_find_page(&thread_current()->spt, pg_round_down(buffer));
	if (pml4_get_page(thread_current()->pml4, buffer) && !find_page->writable)
	{
		sys_exit(-1);
	}
	//
	check_ptr(buffer);
	if (!is_user_vaddr(fd) || fd >= 32 || fd < 0)
	{
		return;
	}
	if (fd == 1)
	{
		sys_exit(-1);
	}
	struct thread *t = thread_current();
	if (t->fd_table[fd] != NULL)
	{
		return file_read(t->fd_table[fd], buffer, size);
	}
	return -1;
}

int sys_write(int fd, const void *buffer, unsigned size)
{
	check_ptr(buffer);
	if (!is_user_vaddr(fd) || fd >= 32 || fd <= 0)
	{
		sys_exit(-1);
	}
	struct thread *t = thread_current();
	if (fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}
	else
	{

		return file_write(t->fd_table[fd], buffer, size);
	}
}

int sys_filesize(int fd)
{
	struct thread *t = thread_current();
	return file_length(t->fd_table[fd]);
}

pid_t sys_fork(const char *thread_name, struct intr_frame *f)
{

	pid_t pid;
	pid = process_fork(thread_name, f);
	// 복제된 프로세스도 자식으로 넣음
	if (pid == TID_ERROR)
	{
		return TID_ERROR;
	}

	return pid;
}

int sys_wait(pid_t pid)
{

	return process_wait(pid);
}

int sys_exec(const char *cmd_line)
{
	check_ptr(cmd_line);
	int file_size = strlen(cmd_line) + 1;
	char *cl_copy = palloc_get_page(PAL_ZERO);
	if (cl_copy == NULL)
	{
		sys_exit(-1);
	}
	strlcpy(cl_copy, cmd_line, file_size);

	if (process_exec(cl_copy) == -1)
	{
		return -1;
	}
	return 0;
}
void sys_seek(int fd, unsigned position)
{
	struct thread *t = thread_current();
	file_seek(t->fd_table[fd], position);
}
unsigned sys_tell(int fd)
{
	struct thread *t = thread_current();
	return file_tell(t->fd_table[fd]);
}
void *sys_mmap(void *addr, size_t length, int writable, int fd, off_t offset)
{
	struct thread *t = thread_current();
	if (fd < 3 || fd > 32)
	{
		sys_exit(-1);
	}
	if (addr == NULL || length == 0 || t->fd_table[fd] == NULL || offset % PGSIZE != 0)
	{
		return NULL;
	}

	return do_mmap(addr, length, writable, t->fd_table[fd], offset);
}
void sys_munmap(void *addr)
{
	// do_munmap(addr);
}