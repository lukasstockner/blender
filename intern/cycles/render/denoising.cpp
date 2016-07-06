#include "denoising.h"

CCL_NAMESPACE_BEGIN

DenoisingSession::DenoisingSession(DenoisingSessionParams &params_)
: params(params_),
  tile_manager(false, 1, params.tile_size, 1, false, true, params.tile_order,
       max(params.device.multi_devices.size(), 1)),
  stats()
{
	        TaskScheduler::init(params.threads);

        device = Device::create(params.device, stats, params.background);

	session_thread = NULL;
	start_time = 0.0;
}
