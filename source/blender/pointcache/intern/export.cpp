/*
 * Copyright 2013, Blender Foundation.
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
 */

#include "export.h"
#include "writer.h"

extern "C" {
#include "DNA_scene_types.h"

#include "BKE_main.h"
#include "BKE_scene.h"
}

namespace PTC {

Exporter::Exporter(Main *bmain, Scene *scene) :
    m_bmain(bmain),
    m_scene(scene),
    m_cancel(false)
{
}

void Exporter::bake(Writer *writer, int start_frame, int end_frame)
{
	thread_scoped_lock(m_mutex);

	for (int cfra = start_frame; cfra <= end_frame; ++cfra) {
		BKE_scene_update_for_newframe(m_bmain, m_scene, m_scene->lay);

		writer->write_sample();

		if (m_cancel)
			break;
	}
}

bool Exporter::cancel() const
{
	thread_scoped_lock(m_mutex);
	return m_cancel;
}

void Exporter::cancel(bool value)
{
	thread_scoped_lock(m_mutex);
	m_cancel = value;
}

} /* namespace PTC */
