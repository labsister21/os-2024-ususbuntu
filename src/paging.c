#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"

__attribute__((aligned(0x1000))) struct PageDirectory _paging_kernel_page_directory = {
    .table = {
        [0] = {
            .flag.present_bit = 1,
            .flag.write_bit = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address = 0,
        },
        [0x300] = {
            .flag.present_bit = 1,
            .flag.write_bit = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address = 0,
        },
    }
};

static struct PageManagerState page_manager_state = {
    .page_frame_map = {
        [0] = true,
        [1 ... PAGE_FRAME_MAX_COUNT - 1] = false
    },
    .free_page_frame_count = PAGE_FRAME_MAX_COUNT,
    // TODO: Initialize page manager state properly
};

void update_page_directory_entry(
    struct PageDirectory* page_dir,
    void* physical_addr,
    void* virtual_addr,
    struct PageDirectoryEntryFlag flag
) {
    uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;
    page_dir->table[page_index].flag = flag;
    page_dir->table[page_index].lower_address = ((uint32_t)physical_addr >> 22) & 0x3FF;
    flush_single_tlb(virtual_addr);
}

void flush_single_tlb(void* virtual_addr) {
    asm volatile("invlpg (%0)" : /* <Empty> */ : "b"(virtual_addr) : "memory");
}



/* --- Memory Management --- */
// TODO: Implement
bool paging_allocate_check(uint32_t amount) {
    // TODO: Check whether requested amount is available
    return amount <= page_manager_state.free_page_frame_count;
}


bool paging_allocate_user_page_frame(struct PageDirectory* page_dir, void* virtual_addr) {
    /*
     * TODO: Find free physical frame and map virtual frame into it
     * - Find free physical frame in page_manager_state.page_frame_map[] using any strategies
     * - Mark page_manager_state.page_frame_map[]
     * - Update page directory with user flags:
     *     > present bit    true
     *     > write bit      true
     *     > user bit       true
     *     > pagesize 4 mb  true
     */

    uint32_t physical_addr = (uint32_t)page_manager_state.free_page_frame_count;

    if (!paging_allocate_check(physical_addr)) {
        return false;
    }

    // uint32_t free_physical_frame;
    // if always make contigous
    // free_physical_frame = PAGE_FRAME_MAX_COUNT - page_manager_state.free_page_frame_count;
    // if find free physical frame using first-fit
    // for (uint32_t i = 0; i < PAGE_FRAME_MAX_COUNT; i++){
    //     if (!page_manager_state.page_frame_map[i]){
    //         free_physical_frame = i;
    //         break;
    //     }
    // }

    // uint32_t physical_addr = (uint32_t)free_physical_frame * PAGE_FRAME_SIZE;
    // if (physical_addr < 128 * 1024 * 1024) {
    struct PageDirectoryEntryFlag userFlag = {
        .present_bit = 1,
        .write_bit = 1,
        .user_bit = 1,
        .use_pagesize_4_mb = 1,
    };
    update_page_directory_entry(page_dir, (uint32_t*)physical_addr, virtual_addr, userFlag);
    // page_manager_state.page_frame_map[free_physical_frame] = true;
    page_manager_state.page_frame_map[PAGE_FRAME_MAX_COUNT - page_manager_state.free_page_frame_count] = true;
    page_manager_state.free_page_frame_count--;

    return true;
    // }

    // return false;
}

bool paging_free_user_page_frame(struct PageDirectory* page_dir, void* virtual_addr) {
    /*
     * TODO: Deallocate a physical frame from respective virtual address
     * - Use the page_dir.table values to check mapped physical frame
     * - Remove the entry by setting it into 0
     */

    uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;
    struct PageDirectoryEntryFlag emptyFlag = {
        .present_bit = 0,
        .write_bit = 0,
        .user_bit = 0,
        .use_pagesize_4_mb = 0,
    };
    page_dir->table[page_index].flag = emptyFlag;
    page_dir->table[page_index].lower_address = 0;
    // deallocate the physical (?)
    // flush_single_tlb(virtual_addr);

    // if always contigous
    // uint32_t last_pageframe_used = PAGE_FRAME_MAX_COUNT - page_manager_state.free_page_frame_count - 1;
    uint32_t last_pageframe_used = PAGE_FRAME_MAX_COUNT - page_manager_state.free_page_frame_count;
    page_manager_state.page_frame_map[last_pageframe_used] = false;
    page_manager_state.free_page_frame_count++;
    // if allocate using first-fit
    // for (uint32_t i = 0; i < PAGE_FRAME_MAX_COUNT; i++){
    //     if (page_manager_state.page_frame_map[i]){
    //         page_manager_state.page_frame_map[i] = false;
    //         page_manager_state.free_page_frame_count--;
    //         break;
    //     }
    // }

    return true;
}