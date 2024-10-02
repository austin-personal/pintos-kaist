/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/vaddr.h"
#include <bitmap.h>
#include "threads/mmu.h"
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

struct bitmap *swap_table;

/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1); // swap 디스크를 가져옴
	// swap 디스크에서 최대 할당할 수 있는 페이지 수
	size_t swap_size = disk_size(swap_disk) / 8;
	swap_table = bitmap_create(swap_size);
}
/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	ASSERT(page != NULL);
	ASSERT(VM_TYPE(type) == VM_ANON);
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;

	anon_page->swap_slot = -1;

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	struct anon_page *anon_page = &page->anon;
	// 익명 페이지 안에 swapout될 때 저장된 swap_slot 정보를 가져옴
	int swap_slot = anon_page->swap_slot;
	// printf("%d\n", swap_slot);
	// 그 정보를 기반으로 해당 swap_slot이 사용중인지 체크
	if (!bitmap_test(swap_table, swap_slot))
	{
		return false;
	}
	// 해당 swap_slot 에 있는 데이터를 kva에 다시 써준다.
	for (int i = 0; i < 8; i++)
	{
		disk_read(swap_disk, swap_slot * 8 + i, kva + i * DISK_SECTOR_SIZE);
	}
	// swap 디스크에서 다시 데이터를 메모리로 가져왔으므로 디스크가 비어있다고 알려줌
	bitmap_set(swap_table, swap_slot, false);
	// anon_page->swap_slot = -1;
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
	// 1. 비어 있는 swap 슬롯을 찾는다.
	int empty_swap_slot = bitmap_scan(swap_table, 0, 1, false);
	if (empty_swap_slot == BITMAP_ERROR)
	{
		// 비어있는 슬롯 없을 경우 false 때림
		return false;
	}
	// 2. 페이지 내용을 swap 디스크에 기록한다.
	for (int i = 0; i < 8; i++)
	{
		disk_write(swap_disk, empty_swap_slot * 8 + i, page->frame->kva + i * DISK_SECTOR_SIZE);
	}
	// 3. 익명 페이지에 swap 슬롯 정보를 기록
	anon_page->swap_slot = empty_swap_slot;
	// 4. 페이지 상태를 업데이트
	bitmap_set(swap_table, empty_swap_slot, true); // 스왑영역 사용중으로 표시
	pml4_set_accessed(thread_current()->pml4, page->va, false);
	pml4_clear_page(thread_current()->pml4, page->va); // 스왑영역으로 들어갔으니 페이지테이블 클리어
	page->frame = NULL;								   // 프레임도 해제
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
}
