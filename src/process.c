#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"


struct ProcessControlBlock* process_get_current_running_pcb_pointer(void) {
    return process_manager_state.active_process_count > 0 ? &(_process_list[process_manager_state.active_process_count - 1]) : NULL;

}

int32_t process_generate_new_pid(void) {
    static uint32_t pid_counter = 0;
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

    new_pcb->metadata.pid = process_generate_new_pid();

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