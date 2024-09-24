/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

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

/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
	/* TODO: Set up the swap_disk. */
	swap_disk = NULL;
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	ASSERT(page != NULL);
	ASSERT(type == VM_ANON);
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	// 페이지가 메모리에 할당되었음을 나타내는 커널 가상 주소 설정
	anon_page->kva = kva;
	// 페이지는 초기화 시점에서 메모리에 있으므로 초기화 상태로 설정
	anon_page->status = PAGE_IN_MEMORY;
	// 초기화 시점에서는 아직 스왑 아웃되지 않았으므로 swap_slot 초기화
	anon_page->swap_slot = (size_t)-1;
	// 페이지는 아직 특정 프레임에 연결되지 않았으므로 frame을 NULL로 설정
	anon_page->frame = NULL;

	page->writable = true;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
}
