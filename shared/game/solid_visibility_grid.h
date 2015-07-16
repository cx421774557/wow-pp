//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software 
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#pragma once

#include "game/visibility_grid.h"
#include "common/grid.h"

namespace wowpp
{
	/// Base class of a visibility grid. A visibility grid manages objects in a world
	/// instance and decides, which objects are visible for other objects.
	class SolidVisibilityGrid : public VisibilityGrid
	{
	public:
		explicit SolidVisibilityGrid(const TileIndex2D &worldSize);
		virtual ~SolidVisibilityGrid();

		virtual VisibilityTile *getTile(const TileIndex2D &position) override;
		virtual VisibilityTile &requireTile(const TileIndex2D &position) override;

	private:

		typedef Grid<VisibilityTile> Tiles;

		Tiles m_tiles;
	};
}