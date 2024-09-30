/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

static bool
lazy_load(struct page *page, void *aux)
{
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct load_info *load_info = (struct load_info *)aux;
	// 파일에서 데이터를 읽기 시작할 위치를 설정.
	file_seek(load_info->file, load_info->offset);
	// 해당 페이지의 커널 가상 주소를 얻음.
	void *kpage = page->frame->kva;
	// file_read를 사용하여 파일에서 데이터를 읽어와 페이지에 로드함.
	if (file_read(load_info->file, kpage, load_info->read_bytes) == NULL)
	{
		return false;
	}
	// 페이지의 나머지 부분을 0으로 채움.
	memset(kpage + load_info->read_bytes, 0, load_info->zero_bytes);
	// printf("do_mmapfdhgdfh\n");
	return true;
}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset)
{
	ASSERT(pg_ofs(addr) == 0);
	// ASSERT(offset % PGSIZE == 0);
	void *start_addr = addr;
	off_t file_len = file_length(file);
	while (length > 0)
	{

		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = file_len < PGSIZE ? file_len : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		// printf("length: %d\n", page_read_bytes);
		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct load_info *aux = malloc(sizeof(struct load_info));
		if (aux == NULL)
		{
			return NULL;
		}
		aux->file = file;
		aux->offset = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;
		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
											writable, lazy_load, aux))
		{
			free(aux);
			return NULL;
		}

		/* Advance. */
		file_len -= PGSIZE;
		length -= PGSIZE;
		offset += page_read_bytes;
		addr += PGSIZE;
		// 승우님 추가
	}
	// printf("addr : %p\n", addr);
	return start_addr;
}

/* Do the munmap */
void do_munmap(void *addr)
{
	spt_remove_page(&thread_current()->spt, addr);
}
