#include "sched.h"

#define GRR_TIMESLICE_MS 100

/* Stub implementations */
static void enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags) 
{
	// Stub
}

static bool dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags) 
{
	return true;  // Stub
}

// ...  more stub functions ... 

const struct sched_class grr_sched_class = {
	.enqueue_task		= enqueue_task_grr,
	.dequeue_task		= dequeue_task_grr,
	.yield_task			= yield_task_grr,
	.yield_to_task		= yield_to_task_grr,

	.wakeup_preempt		= wakeup_preempt_grr,

	.balance			= balance_grr,
	.pick_task			= pick_task_grr,
	.pick_next_task		= pick_next_task_grr,

	.put_prev_task		= put_prev_task_grr,
	.set_next_task		= set_next_task_grr,

#ifdef CONFIG_SMP
	. select_task_rq		= select_task_rq_grr,
	.migrate_task_rq	= migrate_task_rq_grr,
	.task_woken		= task_woken_grr,
	.set_cpus_allowed	= set_cpus_allowed_grr,
	.rq_online		= rq_online_grr,
	.rq_offline		= rq_offline_grr,
	.find_lock_rq		= find_lock_rq_grr,
#endif

	.task_tick		= task_tick_grr,
	.task_fork		= task_fork_grr,
	.task_dead		= task_dead_grr,

	.switching_to		= switching_to_grr,
	.switched_from		= switched_from_grr,
	.switched_to		= switched_to_grr,
	.prio_changed		= prio_changed_grr,

	. get_rr_interval	= get_rr_interval_grr,
	.update_curr		= update_curr_grr,

#ifdef CONFIG_FAIR_GROUP_SCHED
	.task_change_group	= task_change_group_grr,
#endif
};