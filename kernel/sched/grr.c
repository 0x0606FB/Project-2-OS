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


/* Stub implementations */
static void enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags) 
{
	// Stub
}

static bool dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags) 
{
	return true;  // Stub
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
	if (!group || !group->cpus)
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
static void init_grr_rq(struct grr_rq *grr_rq, int cpu)
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

#ifdef CONFIG_FAIR_GROUP_SCHED
	.task_change_group	= task_change_group_grr,
#endif
};