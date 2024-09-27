/* vm.c: Generic interface for virtual memory objects. */
#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "vm/uninit.h"
#include <string.h>
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
	// printf("upage addr : %p\n", upage);
	if (spt_find_page(spt, upage) != NULL)
	{
		goto err;
	}
	/* TODO: Create the page, fetch the initialier according to the VM type,
	 * TODO: and then create "uninit" page struct by calling uninit_new. You
	 * TODO: should modify the field after calling the uninit_new. */
	/* TODO: Insert the page into the spt. */
	/* TODO: 페이지를 생성하고, VM 타입에 따라 초기화 함수를 가져옵니다.
	 * TODO: 그런 다음, uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다.
	 * TODO: uninit_new를 호출한 후에 필드를 수정해야 합니다. */

	/* TODO: 페이지를 spt에 삽입합니다. */
	struct page *new_page = malloc(sizeof(struct page));

	if (new_page == NULL)
	{
		goto err;
	}

	// printf("여긴가?\n");
	// 페이지 초기화 함수 설정
	bool (*page_initializer)(struct page *, enum vm_type, void *);
	// printf("%d\n", type);
	switch (VM_TYPE(type))
	{
	case VM_ANON:
	{
		page_initializer = anon_initializer;
	}
	break;
	case VM_FILE:
	{
		// printf("얍얍ㅇ\n");
		page_initializer = file_backed_initializer;
	}
	break;
	// 필요한 다른 페이지 타입을 추가할 수 있습니다.
	default:
		free(new_page);
		goto err; // 지원하지 않는 페이지 타입인 경우
	}
	// uninit_new를 호출하여 페이지 초기화
	// 무조건 uninit 페이지 생성 .
	// 바뀔 타입 : VM_TYPE(type)
	uninit_new(new_page, upage, init, VM_TYPE(type), aux, page_initializer);
	new_page->writable = writable;
	if (!spt_insert_page(spt, new_page))
	{
		free(new_page);
		goto err;
	}

	return true;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{

	// printf("%p\n", va);
	struct page page;
	/* TODO: Fill this function. */
	struct hash_elem *e;
	page.va = va;
	// spt의 해시 테이블에서 p와 같은 va를 가진 엔트리를 찾습니다.
	e = hash_find(&spt->vm, &page.hash_elem);
	// 해당 엔트리가 존재하면, struct page 포인터를 반환하고, 없으면 NULL을 반환합니다.
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	int succ = false;
	/* TODO: Fill this function. */

	// 보조 페이지 테이블에서 가상 주소가 이미 존재하는지 확인

	// printf("이잉ㅇㅇ\n");
	succ = hash_insert(&spt->vm, &page->hash_elem) == NULL;
	// printf("삽입 성공 : %d\n", succ);

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

	struct frame *frame = malloc(sizeof(struct frame));
	if (frame == NULL)
	{
		PANIC("todo: frame allocation failed");
	}

	uint8_t *kpage = palloc_get_page(PAL_USER);
	if (kpage == NULL)
	{
		PANIC("todo: page allocation failed, implement swap out");
	}

	frame->kva = kpage;				 // 할당된 물리 페이지
	frame->owner = thread_current(); // 현재 스레드가 프레임 소유자
	frame->page = NULL;				 // 페이지 아직 연결되지 않음
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);

	// PANIC("todo");
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
	struct thread *cur = thread_current();
	struct supplemental_page_table *spt UNUSED = &cur->spt;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (!not_present)
	{
		return false;
	}
	// 주소가 유효하다면 페이지 찾음
	struct page *page = spt_find_page(spt, pg_round_down(addr));
	// PANIC("페이지 폴트 page12: %p\n", page->va);
	if (page == NULL)
	{
		return false;
	}
	// 페이지 클레임
	if (!vm_do_claim_page(page))
	{
		return false;
	}

	return true;
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
	/* TODO: Fill this function */
	struct page *page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL)
	{
		return false;
	}
	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();
	if (frame == NULL)
	{
		return false;
	}

	/* Set links */
	frame->page = page;
	page->frame = frame;
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	//&thread_current()->pml4 이거 아님!!! 이거 주소 기호 떼주니까 통과됨!!!!
	if (!pml4_set_page(thread_current()->pml4, page->va, page->frame->kva, page->writable))
	{
		PANIC("매핑실패\n");
		return false;
	}
	// printf("페이지 테이블 매핑 성공: VA=%p, KVA=%p\n", page->va, page->frame->kva);

	return swap_in(page, frame->kva);
	// return true;
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
	struct hash_iterator i;
	hash_first(&i, &src->vm);
	while (hash_next(&i))
	{

		// 가상 주소는 동일하게, 물리주소는 spt 테이블 크기 만큼 다르게 새로 할당
		struct page *parent_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		// printf("parent_page va : %p\n", parent_page->va);
		if (VM_TYPE(parent_page->operations->type) == VM_ANON)
		{

			if (!vm_alloc_page_with_initializer(parent_page->operations->type, parent_page->va, parent_page->writable, NULL, NULL))
			{
				printf("생성실패!\n");
				return false;
			}
			struct page *new_page = spt_find_page(dst, parent_page->va);
			if (!vm_do_claim_page(new_page))
			{
				return false;
			}
			memcpy(new_page->frame->kva, parent_page->frame->kva, PGSIZE);
		}
		else if (VM_TYPE(parent_page->operations->type) == VM_UNINIT)
		{
			void *aux = malloc(sizeof(struct load_info));
			memcpy(aux, parent_page->uninit.aux, sizeof(struct load_info));
			// uninit.type 이건 바뀔 타입 !!!
			if (!vm_alloc_page_with_initializer(parent_page->uninit.type, parent_page->va, parent_page->writable, parent_page->uninit.init, aux))
			{
				free(aux);
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->vm, free_hash_func);
}

// 비트플래그를 사용하여 스택페이지인지 표시함.
bool is_stack_page(struct page *page)
{
	return VM_TYPE(page->operations->type) & VM_STACK;
}

// 페이지가 쓰기 가능한 지 아닌 지 확인하는 함수
// bool is_writable_page(struct page *page)
// {
// 	return (VM_TYPE(page->operations->type) & VM_WRITABLE) != 0;
// }