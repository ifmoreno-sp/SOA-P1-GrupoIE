#include "workload.h"

#include <assert.h>
#include <stddef.h>

void workload_run_units(Task *task, uint32_t units)
{
    assert(task != NULL);
    assert(units <= task->work_units - task->completed_units);

    for (uint32_t i = 0; i < units; i++) {
        task->pi_index++;
        double jd = (double)task->pi_index;
        task->term *= ((2.0 * jd - 1.0) * (2.0 * jd - 1.0)) /
                      ((2.0 * jd) * (2.0 * jd + 1.0));
        task->pi_approx += 2.0 * task->term;
    }

    task->completed_units += units;
}
