//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
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
#include "game/movement_info.h"

namespace wowpp
{
	class MovementInfo;
	struct PendingMovementChange;

	bool validateMovementInfo(UInt16 opCode, const MovementInfo& clientInfo, const MovementInfo& serverInfo, float allowedMoveSpeed);
	bool validateSpeedAck(const PendingMovementChange& change, float receivedSpeed, MovementType& outMoveTypeSent);
	bool validateMoveFlagsOnApply(bool apply, UInt32 flags, UInt32 possiblyAppliedFlags);
	bool validateMovementSpeed(float expectedSpeed, const MovementInfo& clientInfo, const MovementInfo& serverInfo/*, bool isFirstMove = false*/);
}
