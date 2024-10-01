/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/mmu.h"
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
	file_page->fr = page->uninit.aux;
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
	// struct file_page *file_page UNUSED = &page->file;
	// 수정사항 물리 주소(디스크)에 반영
	// struct load_info *find_aux = page->file.fr;
	// if (page->frame != NULL)
	// {

	// 	if (pml4_is_dirty(thread_current()->pml4, page->va))
	// 	{
	// 		file_seek(find_aux->file, find_aux->offset);
	// 		file_write(find_aux->file, page->frame->kva, find_aux->read_bytes);
	// 	}
	// 	pml4_set_dirty(thread_current()->pml4, page->va, false);
	// 	palloc_free_page(page->frame->kva);
	// 	free(page->frame);
	// }
	// pml4_set_accessed(thread_current()->pml4, page->va, false);
	// pml4_clear_page(thread_current()->pml4, page->va);
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
	pml4_set_dirty(thread_current()->pml4, page->va, false);
	// printf("do_mmapfdhgdfh\n");
	return true;
}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset)
{
	// ASSERT(offset % PGSIZE == 0);
	void *start_addr = addr;
	// 파일에 대한 독립적인 참조 얻기
	struct file *reopened_file = file_reopen(file);
	off_t file_len = file_length(reopened_file);
	// 페이지 생성 개수
	int cnt = (int)pg_round_up(length) / PGSIZE;
	long len = length;
	while (len > 0)
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
		aux->file = reopened_file;
		aux->offset = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;
		aux->cnt = cnt;
		// munmap용 파일 데이터 저장
		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
											writable, lazy_load, aux))
		{
			free(aux);
			return NULL;
		}
		/* Advance. */
		file_len -= PGSIZE;
		len -= PGSIZE;
		offset += page_read_bytes;
		addr += PGSIZE;
	}
	return start_addr;
}

/* Do the munmap */
void do_munmap(void *addr)
{
	// printf("HI\n\n");
	struct page *find_page = spt_find_page(&thread_current()->spt, addr);
	struct load_info *find_aux = find_page->file.fr;
	struct file *find_file = find_aux->file;
	if (VM_TYPE(find_page->operations->type) != VM_FILE || find_aux->cnt == 0)
	{
		return;
	}
	int find_cnt = find_aux->cnt;
	// printf("cnt:%d\n", find_cnt);
	while (find_cnt > 0)
	{
		struct page *find_page = spt_find_page(&thread_current()->spt, addr);
		// spt_remove_page(&thread_current()->spt, find_page);
		if (pml4_is_dirty(thread_current()->pml4, find_page->va))
		{
			file_seek(find_aux->file, find_aux->offset);
			file_write(find_aux->file, find_page->frame->kva, find_aux->read_bytes);
		}
		addr += PGSIZE;
		find_cnt--;
	}
	file_close(find_file);
}
