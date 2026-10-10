#pragma once
#include "page_outputs.h"
void page_p4_io_create(lv_obj_t *, outputs_command_fn);
void page_p4_bus_create(lv_obj_t *, outputs_command_fn);
void page_p4_io_locks(bool linked, bool locked);
void page_p4_io_receive(const char *);
void page_p4_io_feedback(const char *);

void page_p4_bus_reset();
