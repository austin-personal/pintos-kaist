#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
//(P2:syscall)
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include <string.h>

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


//(P2:syscall)
void check_ptr(const void *ptr);
void sys_halt(void);
void sys_exit(int status);
int sys_write(int fd, const void *buffer, unsigned size);

bool sys_create (const char *file, unsigned initial_size);
int sys_open(const char *file);
void sys_close(int fd);
int sys_read(int fd, void *buffer, unsigned size);
void sys_seek (int fd, unsigned position);
int sys_filesize(int fd);
unsigned sys_tell(int fd);
bool sys_remove(const char *file);

int wait (pid_t pid);
pid_t fork (const char *thread_name);
int exec (const char *cmd_line);

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

	char *fn_copy;
	int siz;
	
	switch (f->R.rax)
	{
	case SYS_HALT:
		sys_halt();
		break;
	case SYS_EXIT:
		sys_exit(f->R.rdi);
		break;
	// case SYS_FORK:
	// 	// f->R.rax = fork(f->R.rdi, f);
	// 	break;
	// case SYS_EXEC:
	// 	// if (exec(f->R.rdi) == -1)
	// 	// 	exit(-1);
	// 	break;
	// case SYS_WAIT:
	// 	// f->R.rax = process_wait(f->R.rdi);
	// 	break;
	case SYS_CREATE:
		f->R.rax = sys_create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = sys_remove(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = sys_open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = sys_filesize(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = sys_read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = sys_write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		sys_seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = sys_tell(f->R.rdi);
		break;
	case SYS_CLOSE:
		sys_close(f->R.rdi);
		break;
	default:
		thread_exit();
		break;
	}
}

// (P2:syscall)유효포인터 체크 함수
void check_ptr(const void *ptr)
{
	struct thread *cur = thread_current();

	if (ptr > USER_STACK) sys_exit(-1);

	if (!is_user_vaddr(ptr)) sys_exit(-1); // Check whether it is in user stack
	// is_user_vaddr will call is_kernel_vaddr to check whether it is in user stack

	if (pml4_get_page(cur->pml4, ptr) == NULL || ptr == NULL ) sys_exit(-1);
}
// (P2:syscall) Check whether it is correct fd
bool check_fd(int fd)
{
	if (0 <= fd && fd < 32)
	{
		if (thread_current()->fd_table[fd] != NULL)
		{
			return true;
		}
	}
	return false;
}

// (P2:syscall) Harts the process
void sys_halt(void)
{
	power_off();
}


// (P2:syscall) Exits current task
// End the current thread and record exit status.
void sys_exit(int status)
{
	// When Exit, to distinguish that it is proceed on user mode. 
	struct thread *cur = thread_current();
	cur->exit_status = status;
	cur->is_user = true;

	thread_exit(); // It will call process_exit ();
}


// (P2:syscall) Writes size bytes from buffer to the open file fd
int sys_write(int fd, const void *buffer, unsigned size)
{
	check_ptr(buffer);

	struct thread *t = thread_current();

	if (fd == 1) //  fd 1 = 표준 출력(stdout)
	{
		putbuf(buffer, size); // Writes the N characters in BUFFER to the console.
		
		return size;
		
	}
	else if (check_fd(fd))
	{
		
		return file_write(t->fd_table[fd], buffer, size); // Writes size bytes from buffer to the open file fd
	}
	else 
	{
		
		sys_exit(-1);
	}
}

// (P2:syscall)Creates a new file called file initially initial_size bytes in size
bool sys_create (const char *file, unsigned initial_size)
{
	check_ptr(file); //Check if the pointer of file is not correct

	if (file == "" || file == NULL) return false;// Check if the file is Null or nothing

	return filesys_create(file, initial_size); // Create the file. It is not same with sys_open
}

// (P2:syscall) Opens the file called file. Returns a "file descriptor" (fd)
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

// (P2:syscall) Closes file descriptor fd. Exiting or terminating a process.
void sys_close(int fd)
{
	if (!check_fd(fd)) sys_exit(-1);

	if (thread_current()->fd_table[fd] != NULL)
	{
		file_close(thread_current()->fd_table[fd]);
		thread_current()->fd_table[fd] = NULL;
	}
	else
	{
		sys_exit(-1);
	}
}

// (P2:syscall) Reads size bytes from the file open as fd into buffer.
int sys_read(int fd, void *buffer, unsigned size)
{
	check_ptr(buffer);

	if (check_fd(fd))
	{
		if (fd == 1)
		{
			sys_exit(-1);
		}
		struct file *target = thread_current()->fd_table[fd];
		off_t bytes_read = file_read(target, buffer, size);
		return bytes_read;
	}
	else
	{
		sys_exit(-1);
	}
}

// (P2:syscall) Changes the next byte to be read or written in open file fd to position
void sys_seek (int fd, unsigned position)
{
	if (check_fd)
	{
		file_seek(thread_current()->fd_table[fd], position);
	}
	else sys_exit(-1);
}
// (P2:syscall)
int sys_filesize(int fd)
{
	struct thread *t = thread_current();
	return file_length(t->fd_table[fd]);
}

unsigned sys_tell(int fd)
{
	struct thread *t = thread_current();
	return file_tell(t->fd_table[fd]);
}


bool sys_remove(const char *file)
{
	return filesys_remove(file);
}