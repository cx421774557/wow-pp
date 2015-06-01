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

#include "common/constants.h"
#include "network/connector.h"
#include "game_protocol/game_protocol.h"
#include "game_protocol/game_incoming_packet.h"
#include "wowpp_protocol/wowpp_connector.h"
#include "data/data_load_context.h"
#include "game/game_character.h"
#include "common/timer_queue.h"
#include <boost/signals2.hpp>

namespace wowpp
{
	// Forwards
	class WorldInstanceManager;
	class PlayerManager;
	struct Configuration;
	class Project;

	/// This class manages the connection to the realm server.
	class RealmConnector : public pp::IConnectorListener
	{
	public:

		boost::signals2::signal<void (DatabaseId characterId, std::shared_ptr<GameCharacter> character)> worldInstanceEntered;

	public:

		/// 
		/// @param ioService
		/// @param worldInstanceManager
		/// @param config
		/// @param project
		/// @param timer
		explicit RealmConnector(
			boost::asio::io_service &ioService,
			WorldInstanceManager &worldInstanceManager,
			PlayerManager &playerManager,
			const Configuration &config,
			Project &project,
			TimerQueue &timer);
		~RealmConnector();

		/// @copydoc wowpp::pp::IConnectorListener::connectionLost()
		virtual void connectionLost() override;
		/// @copydoc wowpp::pp::IConnectorListener::connectionMalformedPacket()
		virtual void connectionMalformedPacket() override;
		/// @copydoc wowpp::pp::IConnectorListener::connectionPacketReceived()
		virtual void connectionPacketReceived(pp::Protocol::IncomingPacket &packet) override;
		/// @copydoc wowpp::pp::IConnectorListener::connectionEstablished()
		virtual bool connectionEstablished(bool success) override;

		/// 
		/// @param senderId
		/// @param opCode
		/// @param size
		/// @param buffer
		void sendProxyPacket(DatabaseId senderId, UInt16 opCode, UInt32 size, const std::vector<char> &buffer);

	private:

		/// 
		void scheduleConnect();
		/// 
		void tryConnect();
		/// 
		void scheduleKeepAlive();
		/// 
		void onScheduledKeepAlive();

		// Packet handlers
		void handleLoginAnswer(pp::Protocol::IncomingPacket &packet);
		void handleCharacterLogin(pp::Protocol::IncomingPacket &packet);
		void handleProxyPacket(pp::Protocol::IncomingPacket &packet);

	private:

		// Proxy packet handlers
		void handleNameQuery(DatabaseId sender, game::Protocol::IncomingPacket &packet);
		void handleCreatureQuery(DatabaseId sender, game::Protocol::IncomingPacket &packet);

	private:

		// Variables
		boost::asio::io_service &m_ioService;
		WorldInstanceManager &m_worldInstanceManager;
		PlayerManager &m_playerManager;
		const Configuration &m_config;
		Project &m_project;
		TimerQueue &m_timer;
		std::shared_ptr<pp::Connector> m_connection;
		String m_host;
		NetPort m_port;
	};
}