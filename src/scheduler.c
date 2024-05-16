#include "header/scheduler/scheduler.h"

static int32_t current_running_process_index = -1;

int32_t get_next_process_index(void) {
    return (current_running_process_index + 1) % process_manager_state.active_process_count;
}

void scheduler_init(void) {
    process_manager_state.active_process_count = 0;
}

void scheduler_save_context_to_current_running_pcb(struct Context ctx) {
    struct ProcessControlBlock* current_pcb = process_get_current_running_pcb_pointer();
    if (current_pcb != NULL) {
        current_pcb->context = ctx;
    }
}

void scheduler_switch_to_next_process(void) {
    current_running_process_index = get_next_process_index();
    struct ProcessControlBlock* next_pcb = &(_process_list[current_running_process_index]);
    paging_use_page_directory(next_pcb->context.page_directory_virtual_addr);
    process_context_switch(next_pcb->context);
}