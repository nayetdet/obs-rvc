#pragma once

#include <stdbool.h>

typedef struct rvc_worker_process rvc_worker_process_t;

bool rvc_worker_start(rvc_worker_process_t **process);
void rvc_worker_stop(rvc_worker_process_t *process);
