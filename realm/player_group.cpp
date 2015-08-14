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

#include "player_group.h"
#include "game/game_character.h"

namespace wowpp
{
	PlayerGroup::PlayerGroup(PlayerManager &playerManager)
		: m_playerManager(playerManager)
		, m_leaderGUID(0)
		, m_type(group_type::Normal)
		, m_lootMethod(loot_method::GroupLoot)
	{
	}

	void PlayerGroup::create(GameCharacter &leader)
	{
		// Save group leader values
		m_leaderGUID = leader.getGuid();
		m_leaderName = leader.getName();

		// Add the leader as a group member
		auto &newMember = m_members[m_leaderGUID];
		newMember.name = m_leaderName;
		newMember.group = 0;
		newMember.assistant = false;

		// Other checks have already been done in addInvite method, so we are good to go here
		leader.modifyGroupUpdateFlags(group_update_flags::Full, true);
	}

	void PlayerGroup::setLootMethod(LootMethod method)
	{
		// TODO: Notify group members about this change
		m_lootMethod = method;
	}

	bool PlayerGroup::isMember(GameCharacter &character) const
	{
		auto it = m_members.find(character.getGuid());
		return (it != m_members.end());
	}

	void PlayerGroup::setLeader(GameCharacter &newLeader)
	{
		// New character has to be a member of this group
		if (!isMember(newLeader))
		{
			return;
		}

		m_leaderGUID = newLeader.getGuid();
		m_leaderName = newLeader.getName();
	}

	game::PartyResult PlayerGroup::addMember(GameCharacter &member)
	{
		UInt64 guid = member.getGuid();
		if (!m_invited.contains(guid))
		{
			return game::party_result::NotInYourParty;
		}

		// Remove from invite list
		m_invited.remove(guid);

		// Already full?
		if (isFull())
		{
			return game::party_result::PartyFull;
		}

		// Create new group member
		auto &newMember = m_members[guid];
		newMember.status = 1;
		newMember.name = member.getName();
		newMember.group = 0;
		newMember.assistant = false;
		
		member.modifyGroupUpdateFlags(group_update_flags::Full, true);
		auto *memberPlayer = m_playerManager.getPlayerByCharacterGuid(guid);

		// Make sure that all group members know about us
		for (auto &it : m_members)
		{
			auto *player = m_playerManager.getPlayerByCharacterGuid(it.first);
			if (!player)
			{
				// TODO
				continue;
			}

			for (auto &it2 : m_members)
			{
				if (it2.first == it.first)
				{
					continue;
				}

				auto *player2 = m_playerManager.getPlayerByCharacterGuid(it2.first);
				if (!player2)
				{
					// TODO
					continue;
				}

				// Spawn that player for us
				/*std::vector<std::vector<char>> blocks;
				createUpdateBlocks(*player2->getGameCharacter(), blocks);
				player->sendPacket(
					std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(blocks)));*/
				DLOG("SMSG_PARTY_MEMBER_STATS of " << player2->getGameCharacter()->getName() << " to " << player->getGameCharacter()->getName());
				player->sendPacket(
					std::bind(game::server_write::partyMemberStats, std::placeholders::_1, std::cref(*player2->getGameCharacter())));
			}
		}

		// Update group list
		sendUpdate();

		/*
		// Other checks have already been done in addInvite method, so we are good to go here
		broadcastPacket(
			std::bind(game::server_write::partyMemberStats, std::placeholders::_1, std::cref(member)), guid);
			*/

		return game::party_result::Ok;
	}

	game::PartyResult PlayerGroup::addInvite(UInt64 inviteGuid)
	{
		// Can't invite any more members since this group is full already.
		if (isFull())
		{
			return game::party_result::PartyFull;
		}

		m_invited.add(inviteGuid);
		return game::party_result::Ok;
	}

	void PlayerGroup::sendUpdate()
	{
		// Update member status
		for (auto &member : m_members)
		{
			auto *player = m_playerManager.getPlayerByCharacterGuid(member.first);
			if (!player)
			{
				member.second.status = game::group_member_status::Offline;
			}
			else
			{
				member.second.status = game::group_member_status::Online;
			}
		}

		// Send to every group member
		for (auto &member : m_members)
		{
			auto *player = m_playerManager.getPlayerByCharacterGuid(member.first);
			if (!player)
			{
				continue;
			}

			// Send packet
			player->sendPacket(
				std::bind(game::server_write::groupList, std::placeholders::_1, 
					member.first,
					m_type,
					false,
					member.second.group,
					member.second.assistant ? 1 : 0,
					0x50000000FFFFFFFELL, 
					std::cref(m_members),
					m_leaderGUID, 
					m_lootMethod, 
					0,
					0x02,
					0));
		}
	}

}
