#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"

struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX] = { 0 };

struct ProcessManagerState process_manager_state = {
    .active_process_count = 0,
};

struct ProcessControlBlock* process_get_current_running_pcb_pointer(void) {
    return process_manager_state.active_process_count > 0 ? &(_process_list[process_manager_state.active_process_count - 1]) : NULL;
}

int32_t process_generate_new_pid(void) {
    static uint32_t pid_counter = 1;
    return pid_counter++;
}

int32_t process_list_get_inactive_index(void) {
    for (int i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.pid == 0) {
            return i;
        }
    }
    return -1;
}

struct ProcessControlBlock* process_list_get_pcb_by_pid(uint32_t pid) {
    for (int i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.pid == pid) {
            return &(_process_list[i]);
        }
    }
    return NULL;
}

int32_t process_create_user_process(struct FAT32DriverRequest request) {
    int32_t retcode = PROCESS_CREATE_SUCCESS;

    // Step 0: Validasi & pengecekan beberapa kondisi kegagalan
    if (process_manager_state.active_process_count >= PROCESS_COUNT_MAX) {
        retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
        goto exit_cleanup;
    }

    // Ensure entrypoint is not located at kernel's section at higher half
    if ((uint32_t)request.buf >= KERNEL_VIRTUAL_ADDRESS_BASE) {
        retcode = PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT;
        goto exit_cleanup;
    }

    // Check whether memory is enough for the executable and additional frame for user stack
    uint32_t page_frame_count_needed = ceil_div(request.buffer_size + PAGE_FRAME_SIZE, PAGE_FRAME_SIZE);
    if (!paging_allocate_check(page_frame_count_needed) || page_frame_count_needed > PROCESS_PAGE_FRAME_COUNT_MAX) {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }

    // Process PCB 
    int32_t p_index = process_list_get_inactive_index();
    struct ProcessControlBlock* new_pcb = &(_process_list[p_index]);

    // Step 1: Pembuatan virtual address space baru dengan page directory
    struct PageDirectory* page_directory = paging_create_new_page_directory();
    if (!page_directory) {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }

    // Step 2: Membaca dan melakukan load executable dari file system ke memory baru
    int8_t ret = read(request);
    if (ret != 0) {
        retcode = PROCESS_CREATE_FAIL_FS_READ_FAILURE;
        goto exit_cleanup;
    }

    // Step 3: Menyiapkan state & context awal untuk program
    struct Context initial_context = {
        .cpu = {
            .index = {
                .edi = 0,
                .esi = 0,
            },
            .stack = {
                .ebp = 0,
                .esp = 0,
            },
            .general = {
                .ebx = 0,
                .edx = 0,
                .ecx = 0,
                .eax = 0,
            },
            .segment = {
                .gs = 0,
                .fs = 0,
                .es = 0,
                .ds = 0,
            },
        },
        .eflags = CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE,
        .ss = 0,
        .cs = 0,
        .page_directory_virtual_addr = NULL,
        .eip = 0,
    };

    // Update page directory with user flags
    for (int i = 0; i < page_frame_count_needed; i++) {
        void* virtual_addr = (void*)(KERNEL_VIRTUAL_ADDRESS_BASE + i * PAGE_FRAME_SIZE);
        void* physical_addr = (void*)(i * PAGE_FRAME_SIZE);
        struct PageDirectoryEntryFlag user_flag = {
            .present_bit = 1,
            .write_bit = 1,
            .user_bit = 1,
            .use_pagesize_4_mb = 1,
        };
        update_page_directory_entry(page_directory, physical_addr, virtual_addr, user_flag);
        new_pcb->memory.virtual_addr_used[i] = virtual_addr;
    }

    // Update context with new page directory
    initial_context.page_directory_virtual_addr = page_directory;
    new_pcb->context = initial_context;

    // Step 4: Mencatat semua informasi penting process ke metadata PCB
    new_pcb->metadata.pid = process_generate_new_pid();
    new_pcb->memory.page_frame_used_count = page_frame_count_needed;
    new_pcb->metadata.state = PROCESS_STATE_READY;
    memcpy(new_pcb->metadata.name, request.name, strlen(request.name));

    process_manager_state.active_process_count++;

    // Step 5: Mengembalikan semua state register dan memory sama sebelum process creation

exit_cleanup:
    return retcode;
}

/**
 * Destroy process then release page directory and process control block
 *
 * @param pid Process ID to delete
 * @return    True if process destruction success
 */
bool process_destroy(uint32_t pid) {
    struct ProcessControlBlock* pcb = process_list_get_pcb_by_pid(pid);
    if (pcb == NULL) {
        return false;
    }

    // Release page directory
    paging_free_page_directory(pcb->context.page_directory_virtual_addr);

    // Release PCB
    pcb->metadata.pid = 0;
    pcb->metadata.name[0] = '\0';
    pcb->metadata.state = PROCESS_STATE_TERMINATED;

    return true;
}