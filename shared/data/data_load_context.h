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

#include "common/typedefs.h"
#include "templates/basic_template_load_context.h"
#include <boost/filesystem.hpp>
#include <functional>
#include <array>

namespace wowpp
{
	struct MapEntry;
	struct RaceEntry;
	struct ClassEntry;
	struct LevelEntry;
	struct CreatureTypeEntry;
	struct UnitEntry;

	struct DataLoadContext : BasicTemplateLoadContext
	{
		boost::filesystem::path dataPath;
		typedef std::function<const MapEntry * (UInt32)> GetMap;
		typedef std::function<const RaceEntry * (UInt32)> GetRace;
		typedef std::function<const ClassEntry * (UInt32)> GetClass;
		typedef std::function<const LevelEntry * (UInt32)> GetLevel;
		typedef std::function<const CreatureTypeEntry * (UInt32)> GetCreatureType;
		typedef std::function<const UnitEntry * (UInt32)> GetUnit;

		GetMap getMap;
		GetRace getRace;
		GetClass getClass;
		GetCreatureType getCreatureType;
		GetUnit getUnit;

		explicit DataLoadContext(
		    boost::filesystem::path realmDataPath
		);
	};
}
