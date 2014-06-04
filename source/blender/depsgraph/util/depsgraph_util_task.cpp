/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Brecht van Lommel
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include "BLI_utildefines.h"
#include "PIL_time.h"

extern "C" {
#include "BLI_rand.h" /* XXX for eval simulation only, remove eventually */
}

#include "depsgraph_debug.h"
#include "depsgraph_eval.h"
#include "depsnode_component.h"

#include "depsgraph_util_task.h"

/* Task */

/* **** eval simulation **** */
static RNG *deg_eval_sim_rng = NULL;

static void deg_eval_sim_init()
{
	deg_eval_sim_rng = BLI_rng_new((unsigned int)(PIL_check_seconds_timer() * 0x7FFFFFFF));
}

static void deg_eval_sim_free()
{
	BLI_rng_free(deg_eval_sim_rng);
	deg_eval_sim_rng = NULL;
}
/* ******** */

DepsgraphTask::DepsgraphTask(Depsgraph *graph_, OperationDepsNode *node_, eEvaluationContextType context_type_) :
    graph(graph_),
    node(node_),
    context_type(context_type_)
{
}

void DepsgraphTask::run()
{
	if (node->is_noop())
		return;
	
	/* get context */
	// TODO: who initialises this? "Init" operations aren't able to initialise it!!!
	ComponentDepsNode *comp = node->owner; 
	BLI_assert(comp != NULL);
	void *context = comp->contexts[context_type];
	
	/* get "item" */
	// XXX: not everything will use this - some may want something else!
	void *item = &node->ptr;
	
	/* take note of current time */
	double start_time = PIL_check_seconds_timer();
	DepsgraphDebug::task_started(*this);
	
	if (DEG_get_eval_mode() == DEG_EVAL_MODE_SIM) {
		/* simulate work, but actually just take a nap here ... */
		
		int min = 20, max = 30; /* default siesta duration in milliseconds */
		
		int r = BLI_rng_get_int(deg_eval_sim_rng);
		int ms = (int)(min) + r % ((int)(max) - (int)(min));
		PIL_sleep_ms(ms);
	}
	else {
		/* should only be the case for NOOPs, which never get to this point */
		BLI_assert(node->evaluate);
		/* perform operation */
		node->evaluate(context, item);
	}
	
	/* note how long this took */
	double end_time = PIL_check_seconds_timer();
	DepsgraphDebug::task_completed(*this, end_time - start_time);
}

void DepsgraphTask::finish(DepsgraphTaskPool &pool)
{
	deg_schedule_children(pool, graph, context_type, node);
}


/* Task Pool */

DepsgraphTaskPool::DepsgraphTaskPool()
{
	num = 0;
	do_cancel = false;
	
	BLI_mutex_init(&num_mutex);
}

DepsgraphTaskPool::~DepsgraphTaskPool()
{
	stop();
	
	BLI_mutex_end(&num_mutex);
}

void DepsgraphTaskPool::push(Depsgraph *graph, OperationDepsNode *node, eEvaluationContextType context_type)
{
	DepsgraphTaskScheduler::Entry entry(DepsgraphTask(graph, node, context_type), this);
	DepsgraphTaskScheduler::push(entry);
}

#if 0
void DepsgraphTaskPool::wait_work()
{
	BLI_mutex_lock(&num_mutex);

	while(num != 0) {
		BLI_mutex_unlock(&num_mutex);

		BLI_mutex_lock(&DepsgraphTaskScheduler::queue_mutex);

		/* find task from this pool. if we get a task from another pool,
		 * we can get into deadlock */
		DepsgraphTaskScheduler::Entry work_entry;
		bool found_entry = false;
		list<DepsgraphTaskScheduler::Entry>::iterator it;

		for(it = DepsgraphTaskScheduler::queue.begin(); it != DepsgraphTaskScheduler::queue.end(); it++) {
			DepsgraphTaskScheduler::Entry& entry = *it;

			if(entry.pool == this) {
				work_entry = entry;
				found_entry = true;
				DepsgraphTaskScheduler::queue.erase(it);
				break;
			}
		}

		queue_lock.unlock();

		/* if found task, do it, otherwise wait until other tasks are done */
		if(found_entry) {
			/* run task */
			work_entry.task->run();

			/* delete task */
			delete work_entry.task;

			/* notify pool task was done */
			num_decrease(1);
		}

		num_lock.lock();
		if(num == 0)
			break;

		if(!found_entry) {
			THREADING_DEBUG("num==%d, Waiting for condition in DepsgraphTaskPool::wait_work !found_entry\n", num);
			num_cond.wait(num_lock);
			THREADING_DEBUG("num==%d, condition wait done in DepsgraphTaskPool::wait_work !found_entry\n", num);
		}
	}
}
#endif

void DepsgraphTaskPool::wait()
{
	BLI_mutex_lock(&num_mutex);
	
	while(num != 0) {
//		THREADING_DEBUG("num==%d, Waiting for condition in DepsgraphTaskPool::wait\n", num);
		BLI_condition_wait(&num_cond, &num_mutex);
	}
	
	BLI_mutex_unlock(&num_mutex);
}

void DepsgraphTaskPool::cancel()
{
	do_cancel = true;
	
	DepsgraphTaskScheduler::clear(this);
	wait();
	
	do_cancel = false;
}

void DepsgraphTaskPool::stop()
{
	DepsgraphTaskScheduler::clear(this);
	
	BLI_assert(num == 0);
}

bool DepsgraphTaskPool::canceled() const
{
	return do_cancel;
}

void DepsgraphTaskPool::num_decrease(int done)
{
	BLI_mutex_lock(&num_mutex);
	
	BLI_assert(num >= done);
	num -= done;
	
	if(num == 0) {
//		THREADING_DEBUG("num==%d, notifying all in DepsgraphTaskPool::num_decrease\n", num);
		BLI_condition_notify_all(&num_cond);
	}

	BLI_mutex_unlock(&num_mutex);
}

void DepsgraphTaskPool::num_increase()
{
	BLI_mutex_lock(&num_mutex);
	
	num++;
//	THREADING_DEBUG("num==%d, notifying all in DepsgraphTaskPool::num_increase\n", num);
	BLI_condition_notify_all(&num_cond);
	
	BLI_mutex_unlock(&num_mutex);
}


/* Task Scheduler */

DepsgraphTaskScheduler::Threads DepsgraphTaskScheduler::threads;
bool DepsgraphTaskScheduler::do_exit = false;

DepsgraphTaskScheduler::Queue DepsgraphTaskScheduler::queue;
ThreadMutex DepsgraphTaskScheduler::queue_mutex;
ThreadCondition DepsgraphTaskScheduler::queue_cond;

void DepsgraphTaskScheduler::init(int num_threads)
{
	BLI_mutex_init(&queue_mutex);
	
	do_exit = false;
	
	if(num_threads == 0) {
		/* automatic number of threads */
		num_threads = BLI_system_thread_count();
	}
	
	/* launch threads that will be waiting for work */
	threads.resize(num_threads);
	
	for(size_t i = 0; i < threads.size(); i++)
		threads[i] = new Thread(&DepsgraphTaskScheduler::thread_run, i);
	
	deg_eval_sim_init();
}

void DepsgraphTaskScheduler::exit()
{
	/* stop all waiting threads */
	do_exit = true;
	BLI_condition_notify_all(&queue_cond);
	
	/* delete threads */
	for (Threads::const_iterator it = threads.begin(); it != threads.end(); ++it) {
		Thread *t = *it;
		t->join();
		delete t;
	}
	threads.clear();
	
	BLI_mutex_end(&queue_mutex);
	
	deg_eval_sim_free();
}

bool DepsgraphTaskScheduler::thread_wait_pop(Entry& entry)
{
	BLI_mutex_lock(&queue_mutex);
	
	while(queue.empty() && !do_exit)
		BLI_condition_wait(&queue_cond, &queue_mutex);
	
	if(queue.empty()) {
		BLI_assert(do_exit);
		
		BLI_mutex_unlock(&queue_mutex);
		return false;
	}
	
	entry = queue.top();
	queue.pop();
	
	BLI_mutex_unlock(&queue_mutex);
	return true;
}

void DepsgraphTaskScheduler::thread_run(int thread_id)
{
	Entry entry;

	/* todo: test affinity/denormal mask */

	/* keep popping off tasks */
	while(thread_wait_pop(entry)) {
		/* run task */
		entry.task.run();
		
		/* notify pool task was done */
		entry.pool->num_decrease(1);
		
		if (!entry.pool->canceled())
			entry.task.finish(*entry.pool);
	}
}

void DepsgraphTaskScheduler::push(Entry& entry)
{
	entry.pool->num_increase();

	/* add entry to queue */
	BLI_mutex_lock(&queue_mutex);
	
	queue.push(entry);
	BLI_condition_notify_one(&queue_cond);
	
	BLI_mutex_unlock(&queue_mutex);
}

void DepsgraphTaskScheduler::clear(DepsgraphTaskPool *pool)
{
	BLI_mutex_lock(&queue_mutex);
	
	/* erase all tasks from this pool from the queue */
	int done = 0;
	/* Note: priority_queue deliberately does not have iterator access.
	 * To filter the queue we must pop all the elements (O(n)), store them in a vector,
	 * and then reconstruct a filtered queue (using iterator-based copy constructor for efficiency)
	 */
	{
		std::vector<Entry> filtered;
		filtered.reserve(queue.size());
		while (!queue.empty()) {
			const Entry &entry = queue.top();
			if (entry.pool == pool) {
				filtered.push_back(entry);
				++done;
			}
			queue.pop();
		}
		
		queue = Queue(filtered.begin(), filtered.end());
	}
	
	BLI_mutex_unlock(&queue_mutex);
	
	/* notify done */
	pool->num_decrease(done);
}
