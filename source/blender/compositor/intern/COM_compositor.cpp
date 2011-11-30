
#include "BKE_node.h"
extern "C" {
	#include "BLI_threads.h"
}

#include "COM_compositor.h"
#include "COM_ExecutionSystem.h"
#include "COM_WorkScheduler.h"

static ThreadMutex *mutex=NULL;
void COM_execute(bNodeTree *editingtree, int rendering) {
	if (mutex == NULL) { /// TODO: move to blender startup phase
		mutex = new ThreadMutex();
		BLI_mutex_init(mutex);
                WorkScheduler::initialize(); ///TODO: call workscheduler.deinitialize somewhere
	}
	BLI_mutex_lock(mutex);
	if (editingtree->test_break && editingtree->test_break(editingtree->tbh)) {
		// during editing multiple calls to this method can be triggered.
		// make sure one the last one will be doing the work.
		BLI_mutex_unlock(mutex);
		return;

	}

	/* set progress bar to 0% and status to init compositing*/
	editingtree->progress(editingtree->prh, 0.0);
	editingtree->stats_draw(editingtree->sdh, (char*)"Compositing");

	/* initialize execution system */
	ExecutionSystem* system = new ExecutionSystem(editingtree, rendering);
	system->execute();
	delete system;

	BLI_mutex_unlock(mutex);
}
