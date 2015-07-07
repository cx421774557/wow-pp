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

#include "player.h"
#include "player_manager.h"
#include "log/default_log_levels.h"
#include "game/world_instance_manager.h"
#include "game/world_instance.h"
#include "game/each_tile_in_sight.h"
#include <cassert>
#include <limits>

using namespace std;

namespace wowpp
{
	Player::Player(PlayerManager &manager, RealmConnector &realmConnector, WorldInstanceManager &worldInstanceManager, DatabaseId characterId, std::shared_ptr<GameCharacter> character, WorldInstance &instance)
		: m_manager(manager)
		, m_realmConnector(realmConnector)
		, m_worldInstanceManager(worldInstanceManager)
		, m_characterId(characterId)
		, m_character(std::move(character))
		, m_logoutCountdown(worldInstanceManager.getTimerQueue())
		, m_instance(instance)
	{
		m_logoutCountdown.ended.connect(
			std::bind(&Player::onLogout, this));
		m_character->spawned.connect(
			std::bind(&Player::onSpawn, this));
		m_character->despawned.connect(
			std::bind(&Player::onDespawn, this));
		m_character->tileChangePending.connect(
			std::bind(&Player::onTileChangePending, this, std::placeholders::_1, std::placeholders::_2));
	}

	void Player::logoutRequest()
	{
		// Make our character sit down
		auto standState = unit_stand_state::Sit;
		m_character->setByteValue(unit_fields::Bytes1, 0, standState);
		sendProxyPacket(
			std::bind(game::server_write::standStateUpdate, std::placeholders::_1, standState));

		// Setup the logout countdown
		m_logoutCountdown.setEnd(
			getCurrentTime() + (20 * constants::OneSecond));
	}

	void Player::cancelLogoutRequest()
	{
		// Stand up again
		auto standState = unit_stand_state::Stand;
		m_character->setByteValue(unit_fields::Bytes1, 0, standState);
		sendProxyPacket(
			std::bind(game::server_write::standStateUpdate, std::placeholders::_1, standState));

		// Cancel the countdown
		m_logoutCountdown.cancel();
	}

	void Player::onLogout()
	{
		// Remove the character from the world
		m_instance.removeGameObject(*m_character);
		m_character.reset();

		// Notify the realm
		m_realmConnector.notifyWorldInstanceLeft(m_characterId, pp::world_realm::world_left_reason::Logout);
		
		// Remove player
		m_manager.playerDisconnected(*this);
	}
	
	TileIndex2D Player::getTileIndex() const
	{
		TileIndex2D tile;
		
		float x, y, z, o;
		m_character->getLocation(x, y, z, o);

		// Get a list of potential watchers
		auto &grid = m_instance.getGrid();
		grid.getTilePosition(x, y, z, tile[0], tile[1]);
		
		return tile;
	}

	void Player::chatMessage(game::ChatMsg type, game::Language lang, const String &receiver, const String &channel, const String &message)
	{
		if (!m_character)
		{
			WLOG("No character assigned!");
			return;
		}

		float x, y, z, o;
		m_character->getLocation(x, y, z, o);

		// Get a list of potential watchers
		auto &grid = m_instance.getGrid();
		
		// Get tile index
		TileIndex2D tile;
		grid.getTilePosition(x, y, z, tile[0], tile[1]);

		// Get all potential characters
		std::vector<ITileSubscriber*> subscribers;
		forEachTileInSight(
			grid,
			tile,
			[&subscribers](VisibilityTile &tile)
		{
			for (auto * const subscriber : tile.getWatchers().getElements())
			{
				subscribers.push_back(subscriber);
			}
		});

		if (type != game::chat_msg::Say &&
			type != game::chat_msg::Yell)
		{
			WLOG("Unsupported chat mode");
			return;
		}

		// Create the chat packet
		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::Protocol::OutgoingPacket packet(sink);
		game::server_write::messageChat(packet, type, lang, channel, m_character->getGuid(), message, m_character.get());

		const float chatRange = (type == game::chat_msg::Yell) ? 300.0f : 25.0f;
		for (auto * const subscriber : subscribers)
		{
			const float distance = m_character->getDistanceTo(*subscriber->getControlledObject());
			if (distance <= chatRange)
			{
				subscriber->sendPacket(packet, buffer);
			}
		}
	}

	void Player::sendPacket(game::Protocol::OutgoingPacket &packet, const std::vector<char> &buffer)
	{
		// Send the proxy packet to the realm server
		m_realmConnector.sendProxyPacket(m_characterId, packet.getOpCode(), packet.getSize(), buffer);
	}

	void Player::onSpawn()
	{
		// Find our tile
		TileIndex2D tileIndex = getTileIndex();
		VisibilityTile &tile = m_instance.getGrid().requireTile(tileIndex);
		tile.getWatchers().add(this);
	}

	void Player::onDespawn()
	{
		// Find our tile
		TileIndex2D tileIndex = getTileIndex();
		VisibilityTile &tile = m_instance.getGrid().requireTile(tileIndex);
		tile.getWatchers().remove(this);
	}

	void Player::onTileChangePending(VisibilityTile &oldTile, VisibilityTile &newTile)
	{
		// We no longer watch for changes on our old tile
		oldTile.getWatchers().remove(this);

		// Create spawn message blocks
		std::vector<std::vector<char>> spawnBlocks;
		createUpdateBlocks(*m_character, spawnBlocks);

		auto &grid = m_instance.getGrid();

		// Spawn ourself for new watchers
		forEachTileInSightWithout(
			grid,
			newTile.getPosition(),
			oldTile.getPosition(),
			[&spawnBlocks, this](VisibilityTile &tile)
		{
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::compressedUpdateObject(packet, spawnBlocks);

			for(auto * subscriber : tile.getWatchers().getElements())
			{
				assert(subscriber != this);
				subscriber->sendPacket(packet, buffer);
			}

			for (auto *object : tile.getGameObjects().getElements())
			{
				std::vector<std::vector<char>> createBlock;
				createUpdateBlocks(*object, createBlock);

				this->sendProxyPacket(
					std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(createBlock)));
			}
		});

		// Despawn ourself for old watchers
		auto guid = m_character->getGuid();
		forEachTileInSightWithout(
			grid,
			oldTile.getPosition(),
			newTile.getPosition(),
			[guid, this](VisibilityTile &tile)
		{
			// Create the chat packet
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::destroyObject(packet, guid, false);

			for (auto * subscriber : tile.getWatchers().getElements())
			{
				assert(subscriber != this);
				subscriber->sendPacket(packet, buffer);
			}

			for (auto *object : tile.getGameObjects().getElements())
			{
				this->sendProxyPacket(
					std::bind(game::server_write::destroyObject, std::placeholders::_1, object->getGuid(), false));
			}
		});

		// Make us a watcher of the new tile
		newTile.getWatchers().add(this);
	}

}
