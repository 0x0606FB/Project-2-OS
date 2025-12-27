#include "sched.h"

/* Time slice:  100ms converted to jiffies */
#define GRR_TIMESLICE		(HZ / 10)  /* 100ms */

/* Global GRR group structures */
struct grr_group grr_groups[GRR_NGROUPS] = {
	[0] = {
		.group_id = GRR_DEFAULT,
		.ncores = 0,
	},
	[1] = {
		.group_id = GRR_PERFORMANCE,
		.ncores = 0,
	}
};

raw_spinlock_t grr_global_lock = __RAW_SPIN_LOCK_UNLOCKED(grr_global_lock);

/* Helper:  Convert group ID (1 or 2) to array index (0 or 1) */
static inline int grr_group_to_idx(int group_id)
{
	return (group_id == GRR_DEFAULT) ? 0 : 1;
}

/* Helper: Get the group struct for a given group_id */
static inline struct grr_group *grr_get_group(int group_id)
{
	if (group_id < 1 || group_id > GRR_NGROUPS)
		return NULL;
	return &grr_groups[grr_group_to_idx(group_id)];
}

/* Helper: Get GRR task from list_head */
static inline struct task_struct *grr_task_of(struct list_head *grr_list)
{
	return container_of(grr_list, struct task_struct, grr_list);
}

/**
 * find_idlest_cpu_in_group - find the least loaded CPU in a GRR group
 * @group_id: The group to search (GRR_DEFAULT or GRR_PERFORMANCE)
 *
 * Returns:  CPU number with shortest runqueue, or -1 if group is empty
 */
static int find_idlest_cpu_in_group(int group_id)
{
	struct grr_group *group;
	int cpu, best_cpu = -1;
	int best_load = INT_MAX;

	group = grr_get_group(group_id);
	if (!group)
		return -1;

	if (cpumask_empty(group->cpus))
		return -1;

	for_each_cpu(cpu, group->cpus) {
		struct rq *rq = cpu_rq(cpu);
		int load = rq->grr. nr_running;

		if (load < best_load) {
			best_load = load;
			best_cpu = cpu;
		}
	}

	return best_cpu;
}

/**
 * find_busiest_cpu_in_group - find the most loaded CPU in a GRR group
 * @group_id:  The group to search
 *
 * Returns: CPU number with longest runqueue, or -1 if group is empty
 */
static int find_busiest_cpu_in_group(int group_id)
{
	struct grr_group *group;
	int cpu, best_cpu = -1;
	int best_load = -1;

	group = grr_get_group(group_id);
	if (!group)
		return -1;

	if (cpumask_empty(group->cpus))
		return -1;

	for_each_cpu(cpu, group->cpus) {
		struct rq *rq = cpu_rq(cpu);
		int load = rq->grr.nr_running;

		if (load > best_load) {
			best_load = load;
			best_cpu = cpu;
		}
	}

	return best_cpu;
}

/**
 * can_migrate_task_grr - Check if a task can be migrated
 * @p: task to check
 * @dst_cpu: destination CPU
 *
 * Returns: true if task can be migrated to dst_cpu
 */
static bool can_migrate_task_grr(struct task_struct *p, int dst_cpu)
{
	/* Can't migrate if task is currently running */
	if (task_on_cpu(task_rq(p), p))
		return false;

	/* Can't migrate if migration is disabled */
	if (is_migration_disabled(p))
		return false;

	/* Check if task is allowed to run on dst_cpu */
	if (!cpumask_test_cpu(dst_cpu, &p->cpus_mask))
		return false;

	return true;
}

/*
 * Enqueue a task into the GRR runqueue
 */
static void enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct grr_rq *grr_rq = &rq->grr;

	/* Add task to the end of the run queue (FIFO for round-robin) */
	list_add_tail(&p->grr_list, &grr_rq->task_list);

	/* Update counters */
	grr_rq->nr_running++;
	grr_rq->nr_tasks++;

	/* Update global rq counter */
	add_nr_running(rq, 1);

	/* Initialize time slice if this is a fresh enqueue */
	if (!(flags & ENQUEUE_RESTORE))
		p->grr_time_slice = GRR_TIMESLICE;
}

/*
 * Dequeue a task from the GRR runqueue
 */
static bool dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct grr_rq *grr_rq = &rq->grr;

	/* Remove task from the run queue */
	list_del_init(&p->grr_list);

	/* Update counters */
	grr_rq->nr_running--;
	grr_rq->nr_tasks--;

	/* Update global rq counter */
	sub_nr_running(rq, 1);

	return true;
}

/*
 * Yield the current task (requeue at the end)
 */
static void yield_task_grr(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	struct grr_rq *grr_rq = &rq->grr;

	/* Move current task to end of run queue */
	list_move_tail(&curr->grr_list, &grr_rq->task_list);

	/* Reset time slice */
	curr->grr_time_slice = GRR_TIMESLICE;
}

/*
 * Yield to a specific task
 */
static bool yield_to_task_grr(struct rq *rq, struct task_struct *p)
{
	struct grr_rq *grr_rq = &rq->grr;

	/* Check if task is on this runqueue */
	if (task_rq(p) != rq)
		return false;

	/* Move target task to front of run queue */
	list_move(&p->grr_list, &grr_rq->task_list);

	return true;
}

/*
 * Check if the new task should preempt the current task
 * GRR doesn't preempt within the same class - it's pure round-robin
 */
static void wakeup_preempt_grr(struct rq *rq, struct task_struct *p, int flags)
{
	/* 
	 * GRR is non-preemptive within the class. 
	 * A newly woken task just gets added to the queue.
	 * We only preempt if there's no current GRR task running.
	 */
	if (rq->curr->sched_class != &grr_sched_class)
		resched_curr(rq);
}

/*
 * Balance callback - called before picking next task
 * Returns true if a higher-priority class has runnable tasks
 */
static int balance_grr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
	return sched_stop_runnable(rq) || sched_dl_runnable(rq) || sched_rt_runnable(rq);
}

/*
 * Pick the next task to run (without side effects)
 */
static struct task_struct *pick_task_grr(struct rq *rq)
{
	struct grr_rq *grr_rq = &rq->grr;
	struct task_struct *p;

	if (! grr_rq->nr_running)
		return NULL;

	/* Get first task from the list */
	p = grr_task_of(grr_rq->task_list.next);

	return p;
}

/*
 * Pick next task and prepare for context switch
 */
static struct task_struct *pick_next_task_grr(struct rq *rq, struct task_struct *prev)
{
	struct task_struct *next = pick_task_grr(rq);

	if (next && prev->sched_class == &grr_sched_class)
		put_prev_task_grr(rq, prev, next);

	return next;
}

/*
 * Called when switching away from a task
 */
static void put_prev_task_grr(struct rq *rq, struct task_struct *p, struct task_struct *next)
{
	struct grr_rq *grr_rq = &rq->grr;

	/* Update runtime statistics */
	update_curr_grr(rq);

	/* If task is still runnable, move to end of queue (round-robin) */
	if (p->on_rq && p != next) {
		list_move_tail(&p->grr_list, &grr_rq->task_list);
	}
}

/*
 * Called when switching to a task
 */
static void set_next_task_grr(struct rq *rq, struct task_struct *p, bool first)
{
	/* Nothing special needed for GRR */
	p->se.exec_start = rq_clock_task(rq);
}

#ifdef CONFIG_SMP
/*
 * Select a CPU for a waking task - find idlest CPU in task's group
 */
static int select_task_rq_grr(struct task_struct *p, int task_cpu, int flags)
{
	int group_id = p->grr_group;
	int new_cpu;

	/* Find the idlest CPU in the task's group */
	new_cpu = find_idlest_cpu_in_group(group_id);

	/* Make sure the CPU is allowed for this task */
	if (new_cpu >= 0 && cpumask_test_cpu(new_cpu, &p->cpus_mask))
		return new_cpu;

	/* Fall back to current CPU if it's allowed and in the group */
	if (cpumask_test_cpu(task_cpu, &p->cpus_mask)) {
		struct grr_group *group = grr_get_group(group_id);
		if (group && cpumask_test_cpu(task_cpu, group->cpus))
			return task_cpu;
	}

	/* Last resort:  any allowed CPU */
	return cpumask_any(&p->cpus_mask);
}

/*
 * Called when task is migrated to a new CPU
 */
static void migrate_task_rq_grr(struct task_struct *p, int new_cpu)
{
	/* Nothing special needed */
}

/*
 * Called after a task is woken up
 */
static void task_woken_grr(struct rq *rq, struct task_struct *p)
{
	/* Check if we should push this task to another CPU */
	/* For now, select_task_rq_grr handles CPU selection */
}

/*
 * Set allowed CPUs for a task
 */
static void set_cpus_allowed_grr(struct task_struct *p, struct affinity_context *ctx)
{
	set_cpus_allowed_common(p, ctx);
}

/*
 * Called when a CPU comes online
 */
static void rq_online_grr(struct rq *rq)
{
	/* Nothing special needed */
}

/*
 * Called when a CPU goes offline
 */
static void rq_offline_grr(struct rq *rq)
{
	/* Nothing special needed */
}

/*
 * Find and lock the runqueue for a task (for push/pull operations)
 */
static struct rq *find_lock_rq_grr(struct task_struct *p, struct rq *rq)
{
	struct rq *later_rq = NULL;
	int cpu;

	/* Find a CPU to push to */
	cpu = find_idlest_cpu_in_group(p->grr_group);
	if (cpu < 0 || cpu == cpu_of(rq))
		return NULL;

	later_rq = cpu_rq(cpu);

	/* Double-lock the runqueues */
	if (later_rq->grr. nr_running < rq->grr.nr_running) {
		double_lock_balance(rq, later_rq);
		
		if (can_migrate_task_grr(p, cpu))
			return later_rq;
		
		double_unlock_balance(rq, later_rq);
	}

	return NULL;
}
#endif /* CONFIG_SMP */

/*
 * Called on each timer tick
 */
static void task_tick_grr(struct rq *rq, struct task_struct *p, int queued)
{
	struct grr_rq *grr_rq = &rq->grr;

	/* Update runtime accounting */
	update_curr_grr(rq);

	/* Decrement time slice */
	if (p->grr_time_slice > 0)
		p->grr_time_slice--;

	/* Check if time slice expired */
	if (p->grr_time_slice == 0) {
		/* Reset time slice */
		p->grr_time_slice = GRR_TIMESLICE;

		/* If there are other tasks, move to end and reschedule */
		if (grr_rq->nr_running > 1) {
			list_move_tail(&p->grr_list, &grr_rq->task_list);
			resched_curr(rq);
		}
	}
}

/*
 * Called when a new task is forked
 */
static void task_fork_grr(struct task_struct *p)
{
	/* Initialize GRR-specific fields */
	INIT_LIST_HEAD(&p->grr_list);
	p->grr_time_slice = GRR_TIMESLICE;
	
	/* Inherit group from parent */
	if (current->grr_group == GRR_DEFAULT || current->grr_group == GRR_PERFORMANCE)
		p->grr_group = current->grr_group;
	else
		p->grr_group = GRR_DEFAULT;
}

/*
 * Called when a task exits
 */
static void task_dead_grr(struct task_struct *p)
{
	/* Nothing special needed */
}

/*
 * Called when switching TO this scheduling class
 */
static void switching_to_grr(struct rq *rq, struct task_struct *p)
{
	/* Initialize GRR fields if needed */
	if (list_empty(&p->grr_list))
		INIT_LIST_HEAD(&p->grr_list);
	
	p->grr_time_slice = GRR_TIMESLICE;
	
	if (p->grr_group != GRR_DEFAULT && p->grr_group != GRR_PERFORMANCE)
		p->grr_group = GRR_DEFAULT;
}

/*
 * Called when switching FROM this scheduling class
 */
static void switched_from_grr(struct rq *rq, struct task_struct *p)
{
	/* Nothing special needed */
}

/*
 * Called after switch to this class is complete
 */
static void switched_to_grr(struct rq *rq, struct task_struct *p)
{
	/* Check if we should preempt current task */
	if (task_on_rq_queued(p)) {
		if (rq->curr != p) {
			if (rq->curr->sched_class != &grr_sched_class ||
			    p->prio < rq->curr->prio)
				resched_curr(rq);
		}
	}
}

/*
 * Called when task priority changes
 */
static void prio_changed_grr(struct rq *rq, struct task_struct *p, int oldprio)
{
	/* GRR doesn't really use priorities, but handle it anyway */
	if (task_on_rq_queued(p) && rq->curr == p) {
		/* Nothing to do - we're round-robin */
	}
}

/*
 * Return the time slice for a task (in jiffies)
 */
static unsigned int get_rr_interval_grr(struct rq *rq, struct task_struct *p)
{
	return GRR_TIMESLICE;
}

/*
 * Update current task's runtime statistics
 */
static void update_curr_grr(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	u64 now = rq_clock_task(rq);
	u64 delta_exec;

	if (curr->sched_class != &grr_sched_class)
		return;

	delta_exec = now - curr->se.exec_start;
	if (unlikely((s64)delta_exec <= 0))
		return;

	curr->se.sum_exec_runtime += delta_exec;
	curr->se.exec_start = now;
}

/*
 * Initialize GRR scheduler at boot
 */
void __init init_grr_scheduler(void)
{
	int ncpus = num_possible_cpus();
	int half;
	int cpu;

	/* Handle edge cases */
	if (ncpus == 0)
		ncpus = 1;
	
	half = ncpus / 2;
	if (half == 0)
		half = 1;  /* At least 1 CPU per group on uniprocessor */

	pr_info("GRR:  Initializing with %d CPUs\n", ncpus);

	/* Initialize cpumask storage */
	if (! alloc_cpumask_var(&grr_groups[0].cpus, GFP_KERNEL))
		panic("GRR: Failed to allocate cpumask for DEFAULT group");
	if (!alloc_cpumask_var(&grr_groups[1].cpus, GFP_KERNEL))
		panic("GRR: Failed to allocate cpumask for PERFORMANCE group");

	cpumask_clear(grr_groups[0].cpus);
	cpumask_clear(grr_groups[1].cpus);

	/* Split CPUs 50/50 */
	for_each_possible_cpu(cpu) {
		if (cpu < half) {
			cpumask_set_cpu(cpu, grr_groups[0].cpus);
		} else {
			cpumask_set_cpu(cpu, grr_groups[1].cpus);
		}
	}

	/* For uniprocessor:  both groups share the same CPU */
	if (ncpus == 1) {
		cpumask_set_cpu(0, grr_groups[0].cpus);
		cpumask_set_cpu(0, grr_groups[1].cpus);
		grr_groups[0].ncores = 1;
		grr_groups[1].ncores = 1;
	} else {
		grr_groups[0].ncores = half;
		grr_groups[1].ncores = ncpus - half;
	}

	pr_info("GRR: DEFAULT group:  %d CPUs, PERFORMANCE group: %d CPUs\n",
	        grr_groups[0]. ncores, grr_groups[1].ncores);
}

/**
 * init_grr_rq - initialize GRR runqueue for a CPU
 * @grr_rq: GRR runqueue to initialize
 * @cpu: which CPU this rq is for
 */
void init_grr_rq(struct grr_rq *grr_rq, int cpu)
{
	int i;

	INIT_LIST_HEAD(&grr_rq->task_list);
	grr_rq->nr_running = 0;
	grr_rq->nr_tasks = 0;
	grr_rq->curr_group = GRR_DEFAULT;

	/* Determine which group this CPU belongs to */
	for (i = 0; i < GRR_NGROUPS; i++) {
		if (grr_groups[i].cpus && cpumask_test_cpu(cpu, grr_groups[i].cpus)) {
			grr_rq->curr_group = grr_groups[i].group_id;
			break;
		}
	}
}

/*
 * GRR Scheduling Class Definition
 * Note: Use DEFINE_SCHED_CLASS macro for proper linker section placement
 */
DEFINE_SCHED_CLASS(grr) = {
	.enqueue_task		= enqueue_task_grr,
	.dequeue_task		= dequeue_task_grr,
	.yield_task		= yield_task_grr,
	.yield_to_task		= yield_to_task_grr,

	.wakeup_preempt		= wakeup_preempt_grr,

	.balance		= balance_grr,
	.pick_task		= pick_task_grr,
	.pick_next_task		= pick_next_task_grr,

	.put_prev_task		= put_prev_task_grr,
	.set_next_task		= set_next_task_grr,

#ifdef CONFIG_SMP
	. select_task_rq		= select_task_rq_grr,
	.migrate_task_rq	= migrate_task_rq_grr,
	.task_woken		= task_woken_grr,
	.set_cpus_allowed	= set_cpus_allowed_grr,
	.rq_online		= rq_online_grr,
	. rq_offline		= rq_offline_grr,
	.find_lock_rq		= find_lock_rq_grr,
#endif

	.task_tick		= task_tick_grr,
	.task_fork		= task_fork_grr,
	.task_dead		= task_dead_grr,

	.switching_to		= switching_to_grr,
	.switched_from		= switched_from_grr,
	.switched_to		= switched_to_grr,
	.prio_changed		= prio_changed_grr,

	.get_rr_interval	= get_rr_interval_grr,
	.update_curr		= update_curr_grr,
};