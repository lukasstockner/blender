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

typedef std::vector<Tile*> Tiles;

typedef enum TileExecutionState{
	CREATED = 0,
	SCHEDULED = 1,
	FINISHED = 2
} TileExecutionState;

/**
 * @brief A tile is the work that can be scheduled.
 *
 * @see WorkScheduler
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
	 * @brief a list of tiles that needs to be calculated, before this tile can be scheduled
	 */
	Tiles m_depends_on;

	/**
	 * @brief Number of unfinished tiles in the m_depends_on.
	 */
	int m_no_unfinished_tiles;

	/**
	 * @brief a list of tiles that are waiting for me to be finished
	 */
	Tiles m_dependents;

	/**
	 * @brief m_state execution state of this tile.
	 */
	TileExecutionState m_state;

	unsigned int m_tile_number;
public:
	/**
	 * constructor
	 */
	Tile(ExecutionGroup *group, rcti *rect, unsigned int tile_number);
	~Tile();
	/**
	 * @brief get the ExecutionGroup
	 */
	ExecutionGroup *getExecutionGroup() const { return this->m_executionGroup; }

	/**
	 * @brief schedule Schedule this tile for execution into the scheduler.
	 */
	void schedule();

	/**
	 * @brief add_dependent adds a tile to the list of tiles that depends on this tile
	 * @param tile
	 */
	void add_dependent(Tile* tile);

	/**
	 * @brief add_depends_on adds a tile to the list of tiles that this tile depends on
	 * @param tile
	 */
	void add_depends_on(Tile* tile);

	/**
	 * @brief get_rect get the rectangle of this tile
	 * @return reference to the rcti
	 */
	rcti* get_rect() { return this->m_rect; }

	unsigned int get_tile_number() { return this->m_tile_number; }

private:
	/**
	 * @brief execute function called from the workscheduler to execute this tile.
	 */
	void execute();

	friend class ExecutionGroup;
	friend class WorkScheduler;
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:Tile")
#endif
};

#endif
