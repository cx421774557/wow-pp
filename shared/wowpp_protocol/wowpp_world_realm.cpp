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

#include <iostream>
#include "wowpp_world_realm.h"
#include <cassert>

namespace wowpp
{
	namespace pp
	{
		namespace world_realm
		{
			namespace world_write
			{
				void login(pp::OutgoingPacket &out_packet, const std::vector<UInt32> &mapIds, const std::vector<UInt32> &instanceIds)
				{
					out_packet.start(world_packet::Login);
					out_packet
						<< io::write<NetUInt32>(ProtocolVersion)
						<< io::write_dynamic_range<NetUInt8>(mapIds)
						<< io::write_dynamic_range<NetUInt8>(instanceIds);
					out_packet.finish();
				}

				void keepAlive(pp::OutgoingPacket &out_packet)
				{
					out_packet.start(world_packet::KeepAlive);
					out_packet.finish();
				}


				void worldInstanceEntered(pp::OutgoingPacket &out_packet, DatabaseId requesterDbId, UInt64 worldObjectGuid, UInt32 instanceId, UInt32 mapId, UInt32 zoneId, float x, float y, float z, float o)
				{
					out_packet.start(world_packet::WorldInstanceEntered);
					out_packet
						<< io::write<NetDatabaseId>(requesterDbId)
						<< io::write<NetUInt64>(worldObjectGuid)
						<< io::write<NetUInt32>(instanceId)
						<< io::write<NetUInt32>(mapId)
						<< io::write<NetUInt32>(zoneId)
						<< io::write<float>(x)
						<< io::write<float>(y)
						<< io::write<float>(z)
						<< io::write<float>(o);
					out_packet.finish();
				}

				void worldInstanceError(pp::OutgoingPacket &out_packet, DatabaseId requesterDbId, world_instance_error::Type error)
				{
					out_packet.start(world_packet::WorldInstanceError);
					out_packet
						<< io::write<NetDatabaseId>(requesterDbId)
						<< io::write<NetUInt8>(error);
					out_packet.finish();
				}

				void clientProxyPacket(pp::OutgoingPacket &out_packet, DatabaseId characterId, UInt16 opCode, UInt32 size, const std::vector<char> &packetBuffer)
				{
					out_packet.start(world_packet::ClientProxyPacket);
					out_packet
						<< io::write<NetDatabaseId>(characterId)
						<< io::write<NetUInt16>(opCode)
						<< io::write<NetUInt32>(size)
						<< io::write_dynamic_range<NetUInt32>(packetBuffer);
					out_packet.finish();
				}

			}

			namespace realm_write
			{
				void loginAnswer(pp::OutgoingPacket &out_packet, LoginResult result)
				{
					out_packet.start(realm_packet::LoginAnswer);
					out_packet
						<< io::write<NetUInt32>(ProtocolVersion)
						<< io::write<NetUInt8>(result);
					out_packet.finish();
				}

				void characterLogIn(pp::OutgoingPacket &out_packet, DatabaseId characterRealmId, const GameCharacter &character)
				{
					out_packet.start(realm_packet::CharacterLogIn);
					out_packet
						<< io::write<NetDatabaseId>(characterRealmId)
						<< character;
					out_packet.finish();
				}

				void clientProxyPacket(pp::OutgoingPacket &out_packet, DatabaseId characterId, UInt16 opCode, UInt32 size, const std::vector<char> &packetBuffer)
				{
					out_packet.start(realm_packet::ClientProxyPacket);
					out_packet
						<< io::write<NetDatabaseId>(characterId)
						<< io::write<NetUInt16>(opCode)
						<< io::write<NetUInt32>(size)
						<< io::write_dynamic_range<NetUInt32>(packetBuffer);
					out_packet.finish();
				}
			}

			namespace world_read
			{
				bool login(io::Reader &packet, UInt32 &out_protocol, std::vector<UInt32> &out_mapIds, std::vector<UInt32> &out_instanceIds)
				{
					return packet
						>> io::read<NetUInt32>(out_protocol)
						>> io::read_container<NetUInt8>(out_mapIds)
						>> io::read_container<NetUInt8>(out_instanceIds);
				}

				bool keepAlive(io::Reader &packet)
				{
					return packet;
				}
				
				bool worldInstanceEntered(io::Reader &packet, DatabaseId &out_requesterDbId, UInt64 &out_worldObjectGuid, UInt32 &out_instanceId, UInt32 &out_mapId, UInt32 &out_zoneId, float &out_x, float &out_y, float &out_z, float &out_o)
				{
					return packet
						>> io::read<NetDatabaseId>(out_requesterDbId)
						>> io::read<NetUInt64>(out_worldObjectGuid)
						>> io::read<NetUInt32>(out_instanceId)
						>> io::read<NetUInt32>(out_mapId)
						>> io::read<NetUInt32>(out_zoneId)
						>> io::read<float>(out_x)
						>> io::read<float>(out_y)
						>> io::read<float>(out_z)
						>> io::read<float>(out_o);
				}

				bool worldInstanceError(io::Reader &packet, DatabaseId &out_requesterDbId, world_instance_error::Type &out_error)
				{
					return packet
						>> io::read<NetDatabaseId>(out_requesterDbId)
						>> io::read<NetUInt8>(out_error);
				}

				bool clientProxyPacket(io::Reader &packet, DatabaseId &out_characterId, UInt16 &out_opCode, UInt32 &out_size, std::vector<char> &out_packetBuffer)
				{
					return packet
						>> io::read<NetDatabaseId>(out_characterId)
						>> io::read<NetUInt16>(out_opCode)
						>> io::read<NetUInt32>(out_size)
						>> io::read_container<NetUInt32>(out_packetBuffer);
				}

			}

			namespace realm_read
			{
				bool loginAnswer(io::Reader &packet, UInt32 &out_protocol, LoginResult &out_result)
				{
					return packet
						>> io::read<NetUInt32>(out_protocol)
						>> io::read<NetUInt8>(out_result);
				}

				bool characterLogIn(io::Reader &packet, DatabaseId &out_characterRealmId, GameCharacter *out_character)
				{
					assert(out_character);

					return packet
						>> io::read<NetDatabaseId>(out_characterRealmId)
						>> *out_character;
				}

				bool clientProxyPacket(io::Reader &packet, DatabaseId &out_characterId, UInt16 &out_opCode, UInt32 &out_size, std::vector<char> &out_packetBuffer)
				{
					return packet
						>> io::read<NetDatabaseId>(out_characterId)
						>> io::read<NetUInt16>(out_opCode)
						>> io::read<NetUInt32>(out_size)
						>> io::read_container<NetUInt32>(out_packetBuffer);
				}

			}
		}
	}
}