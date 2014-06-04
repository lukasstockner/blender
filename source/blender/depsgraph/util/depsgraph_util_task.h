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

#ifndef __DEPSGRAPH_UTIL_TASK_H__
#define __DEPSGRAPH_UTIL_TASK_H__

#include <vector>

#include "DEG_depsgraph.h" /* XXX for eEvaluationContextType ... could be moved perhaps */
#include "depsgraph.h"
#include "depsnode_operation.h"

#include "depsgraph_util_priority_queue.h"
#include "depsgraph_util_thread.h"

class Depsgraph;
class OperationDepsNode;

class DepsgraphTaskPool;
class DepsgraphTaskScheduler;

/* Task
 *
 * Base class for tasks to be executed in threads. */
class DepsgraphTask
{
public:
	Depsgraph *graph;
	OperationDepsNode *node;
	eEvaluationContextType context_type;
	
	DepsgraphTask(Depsgraph *graph_, OperationDepsNode *node_, eEvaluationContextType context_type_);
	
	void run();
	void finish(DepsgraphTaskPool &pool);
};

/* Task Pool
 *
 * Pool of tasks that will be executed by the central TaskScheduler.
 * For each pool, we can wait for all tasks to be done,
 * or cancel them before they are done.
 */
class DepsgraphTaskPool
{
public:
	DepsgraphTaskPool();
	~DepsgraphTaskPool();
	
	void push(Depsgraph *graph, OperationDepsNode *node, eEvaluationContextType context_type);
	
#if 0
	/* XXX wait_work seems to duplicate scheduler code,
	 * instead can just put the calling thread to sleep
	 * and let the scheduler handle tasks
	 */
	void wait_work();		/* work and wait until all tasks are done */
#endif
	void wait();			/* wait until all tasks are done */
	void cancel();			/* cancel all tasks, keep worker threads running */
	void stop();			/* stop all worker threads */
	
	bool canceled() const;	/* for worker threads, test if canceled */
	
protected:
	friend class DepsgraphTaskScheduler;
	
	void num_decrease(int done);
	void num_increase();
	
	ThreadMutex num_mutex;
	ThreadCondition num_cond;
	
	int num;
	bool do_cancel; /* XXX do we need a rw mutex for this? unlikely, but ... */
};

/* Task Scheduler
 * 
 * Central scheduler that holds running threads ready to execute tasks. A singe
 * queue holds the task from all pools. */
class DepsgraphTaskScheduler
{
public:
	static void init(int num_threads = 0);
	static void exit();
	
	/* number of threads that can work on task */
	static int num_threads() { return threads.size(); }
	
protected:
	friend class DepsgraphTaskPool;

	typedef std::vector<Thread*> Threads;
	struct Entry {
		DepsgraphTask task;
		DepsgraphTaskPool *pool;
		
		Entry() :
		    task(DepsgraphTask(NULL, NULL, DEG_EVALUATION_CONTEXT_VIEWPORT)),
		    pool(NULL)
		{}
		Entry(const DepsgraphTask &task, DepsgraphTaskPool *pool) :
		    task(task),
		    pool(pool)
		{}
	};
	
	struct cmp_entry {
		bool operator() (const Entry &a, const Entry &b)
		{
			return a.task.node->eval_priority < b.task.node->eval_priority;
		}
	};
	typedef priority_queue<Entry, std::vector<Entry>, cmp_entry> Queue;
	
	static Threads threads;             /* worker threads */
	static bool do_exit;

	static Queue queue;
	static ThreadMutex queue_mutex;
	static ThreadCondition queue_cond;

	static void thread_run(int thread_id);
	static bool thread_wait_pop(Entry& entry);

	static void push(Entry& entry);
	static void clear(DepsgraphTaskPool *pool);
};

#endif /* __DEPSGRAPH_UTIL_TASK_H__ */
