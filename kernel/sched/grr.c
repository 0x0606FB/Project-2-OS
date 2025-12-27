#include "sched.h"

#define GRR_TIMESLICE_MS 100


/* Global GRR group structures */
struct grr_group grr_groups[GRR_NGROUPS] = {
	[0] = {
		.group_id = GRR_DEFAULT,
		.ncores = 0,  /* Will be initialized at boot */
	},
	[1] = {
		.group_id = GRR_PERFORMANCE,
		.ncores = 0,  /* Will be initialized at boot */
	}
};

raw_spinlock_t grr_global_lock = __RAW_SPIN_LOCK_UNLOCKED(grr_global_lock);

/* Helper:  Convert group ID (1 or 2) to array index (0 or 1) */
static inline int grr_group_to_idx(int group_id)
{
	return (group_id == GRR_DEFAULT) ? 0 : 1;
}

/* Helper:  Get the group struct for a given group_id */
static inline struct grr_group *grr_get_group(int group_id)
{
	if (group_id < 1 || group_id > GRR_NGROUPS)
		return NULL;
	return &grr_groups[grr_group_to_idx(group_id)];
}


/*
 * Enqueue a task into the GRR runqueue
 */
static void enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct grr_rq *grr_rq = &rq->grr;
	
	/* TODO: Add task to grr_rq->task_list */
	/* TODO: Update counters */
	/* TODO: Call add_nr_running(rq, 1) */
}

/*
 * Dequeue a task from the GRR runqueue
 */
static bool dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct grr_rq *grr_rq = &rq->grr;
	
	/* TODO: Remove task from grr_rq->task_list */
	/* TODO: Update counters */
	/* TODO: Call sub_nr_running(rq, 1) */
	return true;
}

/*
 * Yield the current task (requeue at the end)
 */
static void yield_task_grr(struct rq *rq)
{
	/* TODO: Move current task to end of run queue */
}

/*
 * Yield to a specific task
 */
static bool yield_to_task_grr(struct rq *rq, struct task_struct *p)
{
	return false;  /* Not supported for now */
}

/*
 * Check if the new task should preempt the current task
 */
static void wakeup_preempt_grr(struct rq *rq, struct task_struct *p, int flags)
{
	/* GRR is round-robin, typically no preemption within same class */
}

/*
 * Balance callback - called before picking next task
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
	
	if (! grr_rq->nr_running)
		return NULL;
	
	/* TODO: Return first task from grr_rq->task_list */
	return NULL;
}

/*
 * Pick next task and prepare for context switch
 */
static struct task_struct *pick_next_task_grr(struct rq *rq, struct task_struct *prev)
{
	return pick_task_grr(rq);
}

/*
 * Called when switching away from a task
 */
static void put_prev_task_grr(struct rq *rq, struct task_struct *p, struct task_struct *next)
{
	/* TODO: Update task state, move to end of queue if still runnable */
}

/*
 * Called when switching to a task
 */
static void set_next_task_grr(struct rq *rq, struct task_struct *p, bool first)
{
	/* TODO: Set up task as current */
}

#ifdef CONFIG_SMP
/*
 * Select a CPU for a waking task
 */
static int select_task_rq_grr(struct task_struct *p, int task_cpu, int flags)
{
	/* TODO: Find idlest CPU in task's group */
	return task_cpu;
}

/*
 * Called when task is migrated to a new CPU
 */
static void migrate_task_rq_grr(struct task_struct *p, int new_cpu)
{
	/* TODO: Handle migration if needed */
}

/*
 * Called after a task is woken up
 */
static void task_woken_grr(struct rq *rq, struct task_struct *p)
{
	/* TODO: Check if migration is needed */
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
	/* TODO: Handle CPU coming online */
}

/*
 * Called when a CPU goes offline
 */
static void rq_offline_grr(struct rq *rq)
{
	/* TODO: Handle CPU going offline */
}

/*
 * Find and lock the runqueue for a task
 */
static struct rq *find_lock_rq_grr(struct task_struct *p, struct rq *rq)
{
	return NULL;  /* TODO: Implement for push/pull operations */
}
#endif /* CONFIG_SMP */

/*
 * Called on each timer tick
 */
static void task_tick_grr(struct rq *rq, struct task_struct *p, int queued)
{
	/* TODO:  Decrement time slice, reschedule if expired */
	/* Time slice is 100ms */
}

/*
 * Called when a new task is forked
 */
static void task_fork_grr(struct task_struct *p)
{
	/* TODO:  Initialize GRR-specific task fields */
}

/*
 * Called when a task exits
 */
static void task_dead_grr(struct task_struct *p)
{
	/* Cleanup if needed */
}

/*
 * Called when switching TO this scheduling class
 */
static void switching_to_grr(struct rq *rq, struct task_struct *p)
{
	/* TODO: Initialize task for GRR scheduling */
}

/*
 * Called when switching FROM this scheduling class
 */
static void switched_from_grr(struct rq *rq, struct task_struct *p)
{
	/* Cleanup if needed */
}

/*
 * Called after switch to this class is complete
 */
static void switched_to_grr(struct rq *rq, struct task_struct *p)
{
	/* TODO: Check if preemption is needed */
}

/*
 * Called when task priority changes
 */
static void prio_changed_grr(struct rq *rq, struct task_struct *p, int oldprio)
{
	/* GRR doesn't use priority, but may need to handle */
}

/*
 * Return the time slice for a task
 */
static unsigned int get_rr_interval_grr(struct rq *rq, struct task_struct *p)
{
	return GRR_TIMESLICE_MS * (HZ / 1000);  /* Convert ms to jiffies */
}

/*
 * Update current task's runtime statistics
 */
static void update_curr_grr(struct rq *rq)
{
	/* TODO: Update runtime accounting */
}

void __init init_grr_scheduler(void)
{
	int ncpus = num_online_cpus();
	int half = ncpus / 2;
	int cpu;
	
	printk(KERN_INFO "GRR:  Initializing with %d CPUs\n", ncpus);
	
	/* Initialize cpumask storage */
	alloc_cpumask_var(&grr_groups[0].cpus, GFP_KERNEL);
	alloc_cpumask_var(&grr_groups[1].cpus, GFP_KERNEL);
	
	cpumask_clear(grr_groups[0]. cpus);
	cpumask_clear(grr_groups[1].cpus);
	
	/* Split CPUs 50/50 */
	for (cpu = 0; cpu < ncpus; cpu++) {
		if (cpu < half) {
			cpumask_set_cpu(cpu, grr_groups[0]. cpus);
		} else {
			cpumask_set_cpu(cpu, grr_groups[1].cpus);
		}
	}
	
	grr_groups[0].ncores = half;
	grr_groups[1].ncores = ncpus - half;
	
	printk(KERN_INFO "GRR: DEFAULT group:  %d CPUs, PERFORMANCE group: %d CPUs\n",
	       grr_groups[0].ncores, grr_groups[1].ncores);
}

/**
 * find_idlest_cpu_in_group - find the least loaded CPU in a GRR group
 * @group_id: The group to search (GRR_DEFAULT or GRR_PERFORMANCE)
 * 
 * Returns:  CPU number with shortest runqueue, or -1 if group is empty
 * 
 * NOTE: Called with rq->lock held or from context where runqueues are stable. 
 */
static int find_idlest_cpu_in_group(int group_id)
{
	struct grr_group *group;
	int cpu, best_cpu = -1;
	int best_load = INT_MAX;
	
	group = grr_get_group(group_id);
	if (!group || cpumask_empty(group->cpus))
		return -1;
	
	/* Find CPU with fewest runnable tasks */
	for_each_cpu(cpu, group->cpus) {
		struct rq *rq = cpu_rq(cpu);
		int load = rq->nr_running;  /* Total runnable tasks on CPU */
		
		if (load < best_load) {
			best_load = load;
			best_cpu = cpu;
		}
	}
	
	return best_cpu;
}

/**
 * init_grr_rq - initialize GRR runqueue for a CPU
 * @grr_rq: GRR runqueue to initialize
 * @cpu: which CPU this rq is for
 */
void init_grr_rq(struct grr_rq *grr_rq, int cpu)
{
	INIT_LIST_HEAD(&grr_rq->task_list);
	grr_rq->nr_running = 0;
	grr_rq->nr_tasks = 0;
	
	/* Determine which group this CPU belongs to */
	for (int i = 0; i < GRR_NGROUPS; i++) {
		if (cpumask_test_cpu(cpu, grr_groups[i]. cpus)) {
			grr_rq->curr_group = grr_groups[i].group_id;
			break;
		}
	}
}

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
#endif
};