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

#include "realm.h"
#include "realm_manager.h"
#include "player_manager.h"
#include "player.h"
#include "wowpp_protocol/wowpp_realm_login.h"
#include "common/sha1.h"
#include "log/default_log_levels.h"
#include "database.h"
#include <iostream>
#include <cassert>
#include <limits>

using namespace std;

namespace wowpp
{
	Realm::Realm(RealmManager &manager, PlayerManager &playerManager, IDatabase &database, std::shared_ptr<Client> connection, const String &address)
		: m_manager(manager)
		, m_playerManager(playerManager)
		, m_database(database)
		, m_connection(std::move(connection))
		, m_address(address)
		, m_authed(false)
	{
		assert(m_connection);

		m_connection->setListener(*this);
	}

	void Realm::connectionLost()
	{
		WLOG("Realm " << (m_authed ? m_name : m_address) << " disconnected");

		// Flag realm as offline
		if (m_authed)
		{
			m_database.setRealmOffline(m_realmID);
		}

		destroy();
	}

	void Realm::destroy()
	{
		m_connection->resetListener();
		m_connection.reset();

		m_manager.realmDisconnected(*this);
	}

	void Realm::connectionMalformedPacket()
	{
		WLOG("Realm " << m_address << " sent malformed packet");
		destroy();
	}

	void Realm::connectionPacketReceived(pp::IncomingPacket &packet)
	{
		const auto packetId = packet.getId();

		switch (packetId)
		{
			case pp::realm_login::realm_packet::Login:
			{
				handleLogin(packet);
				break;
			}

			case pp::realm_login::realm_packet::PlayerLogin:
			{
				handlePlayerLogin(packet);
				break;
			}

			default:
			{
				// Log unknown or unhandled packet
				WLOG("Received unknown packet " << static_cast<UInt32>(packetId) 
					<< " from realm at " << m_address);
				break;
			}
		}
	}

	void Realm::handleLogin(pp::IncomingPacket &packet)
	{
		using namespace pp::realm_login;

		String internalName, password, visibleName, host;
		NetPort port;

		if (!realm_read::login(packet,
			internalName,
			std::numeric_limits<UInt8>::max(),
			password,
			std::numeric_limits<UInt8>::max(),
			visibleName,
			std::numeric_limits<UInt8>::max(),
			host,
			std::numeric_limits<UInt8>::max(),
			port))
		{
			return;
		}

		// Check for a realm using this name already
		LoginResult result = login_result::AlreadyLoggedIn;
		if (!m_manager.getRealmByInternalName(internalName))
		{
			// Try to log in
			result = m_database.realmLogIn(m_realmID, internalName, password);
			if (result == login_result::Success)
			{
				// Mark realm as online
				if (!m_database.setRealmOnline(m_realmID, visibleName, host, port))
				{
					ELOG("Could not update realm in database!");
					result = login_result::ServerError;
				}
				else
				{
					// Save internal name
					m_name = internalName;

					ILOG("Realm " << m_name << " successfully authenticated");

					// Update entry values
					m_entry.name = visibleName;
					m_entry.port = port;
					m_entry.address = host;
					m_entry.flags = auth::realm_flags::None;
					m_entry.icon = 0;

					// Realm is authentificated
					m_authed = true;
				}
			}
		}

		// Send realm answer
		m_connection->sendSinglePacket(
			std::bind(
				login_write::loginResult, std::placeholders::_1, result));
	}

	void Realm::handlePlayerLogin(pp::IncomingPacket &packet)
	{
		using namespace pp::realm_login;

		if (!m_authed)
		{
			return;
		}

		String accountName;
		if (!realm_read::playerLogin(packet, accountName))
		{
			return;
		}

		// Write to the log
		DLOG("Player " << accountName << " tries to login on realm " << m_name);

		// Check if the player is logged in
		auto player = m_playerManager.getPlayerByAccountName(accountName);
		if (player)
		{
			// Session is always valid here since getPlayerByAccount already checks it
			const auto session = player->getSession();
			assert(session != nullptr);
			
			// Check if the player is not already logged in to a realm
			if (session->hasEnteredRealm())
			{
				//TODO: Player already entered a realm - what to do now?
				
			}
			else
			{
				// Send success
				m_connection->sendSinglePacket(
					std::bind(
						login_write::playerLoginSuccess, 
						std::placeholders::_1, 
						session->getUserName(), 
						session->getUserId(), 
						session->getKey(),
						session->getV(),
						session->getS()));
				return;
			}
		}

		// Send failure
		m_connection->sendSinglePacket(
			std::bind(login_write::playerLoginFailure, std::placeholders::_1, std::cref(accountName)));
	}

}