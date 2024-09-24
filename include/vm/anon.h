#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

enum page_status
{
    PAGE_IN_MEMORY,
    PAGE_SWAPPED,
    PAGE_UNINITIALIZED
};

struct anon_page
{
    size_t swap_slot;
    struct frame *frame;
    enum page_status status;
    void *kva;
};

void vm_anon_init(void);
bool anon_initializer(struct page *page, enum vm_type type, void *kva);

#endif
