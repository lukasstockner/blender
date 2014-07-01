/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

class Tile;

#ifndef _COM_Tile_h_
#define _COM_Tile_h_
class ExecutionGroup;
#include "COM_ExecutionGroup.h"
class WorkScheduler;
#include <vector>

/**
 * @brief A tile is the work that can be scheduled.
 *
 * The scheduling is triggered in the ExecutionGroup and it will be executed by the CPUDevice or OpenCLDevice.
 * The scheduling is implemented in the WorkScheduler
 *
 * @see ExecutionGroup
 * @see WorkScheduler
 * @see CPUDevice
 * @see OpenCLDevice
 */
class Tile {
private:
	/**
	 * @brief executionGroup with the operations-setup to be evaluated
	 */
	ExecutionGroup *m_executionGroup;

	/**
	 * @brief rcti that this tile calculates from an ExecutionGroup
	 */
	rcti *m_rect;

	/**
	 * @brief m_tile_number this is the tile number/chunk number of the old scheduling system.
	 * It is currently used as interface data towards the ExecutionGroup. This interface will be changed
	 * in the future and thereby will make this data unneeded.
	 */
	unsigned int m_tile_number;
public:
	/**
	 * @brief constructor
	 */
	Tile(ExecutionGroup *group, rcti *rect, unsigned int tile_number);

	/**
	  * @brief descructor
	  */
	~Tile();

	/**
	 * @brief get the ExecutionGroup
	 */
	ExecutionGroup *getExecutionGroup() const { return this->m_executionGroup; }

	/**
	 * @brief get_rect get the rectangle of this tile
	 * @return reference to the rcti
	 */
	rcti* get_rect() { return this->m_rect; }

	/**
	 * @brief get_tile_number
	 * @return return the chunk number of this tile
	 */
	unsigned int get_tile_number() { return this->m_tile_number; }

private:
	friend class ExecutionGroup;
	friend class WorkScheduler;
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:Tile")
#endif
};

#endif
