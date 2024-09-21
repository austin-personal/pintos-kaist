/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	// 현재 쓰레드 페이지 테이블에서 upage와 같은 가상주소를 가진 페이지를 못 찾았을 때
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* TODO: Insert the page into the spt. */
		/* TODO: 페이지를 생성하고, VM 타입에 따라 초기화 함수를 가져옵니다.
		 * TODO: 그런 다음, uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다.
		 * TODO: uninit_new를 호출한 후에 필드를 수정해야 합니다. */

		/* TODO: 페이지를 spt에 삽입합니다. */
		struct page *new_page;
		switch (VM_TYPE(type))
		{
		case VM_ANON:
		{
			init = anon_initializer;
		}
		break;
		case VM_FILE:
		{
			init = file_backed_initializer;
		}
		break;
		default:
			return false;
		}
		new_page = malloc(sizeof(struct page));
		if (new_page == NULL)
			goto err;

		uninit_new(new_page, upage, init, type, aux, init);

		if (!spt_insert_page(spt, new_page))
		{

			free(new_page);
			goto err;
		}
	}

err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function. */
	struct hash_iterator i;

	hash_first(&i, &spt->vm);
	while (hash_next(&i))
	{
		page = hash_entry(hash_cur(&i), struct page, hash_elem);
		if (page->va == va)
		{
			break;
		}
	}
	return page;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	int succ = false;
	/* TODO: Fill this function. */

	// 보조 페이지 테이블에서 가상 주소가 이미 존재하는지 확인
	struct hash_elem *existing_elem = hash_find(&spt->vm, &page->hash_elem);

	if (existing_elem == NULL)
	{
		// 가상 주소가 존재하지 않으면 페이지를 삽입
		hash_insert(&spt->vm, &page->hash_elem);
		succ = true;
	}
	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void)
{
	// 슬아 추가
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	ASSERT(frame != NULL);
	frame->kva = palloc_get_page(PAL_USER);
	ASSERT(frame->page == NULL);
	PANIC("todo");
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	page = spt_find_page(spt, addr);
	if (page == NULL)
	{
		return false;
	}
	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL || !vm_do_claim_page(page))
	{
		return false;
	}
	return true;
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (!spt_insert_page(&thread_current()->spt, page))
	{
		return false;
	};
	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{

	hash_init(&spt->vm, page_hash_func, page_less_func, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

// 비트플래그를 사용하여 스택페이지인지 표시함.
bool is_stack_page(struct page *page)
{
	return VM_TYPE(page->operations->type) & VM_STACK;
}