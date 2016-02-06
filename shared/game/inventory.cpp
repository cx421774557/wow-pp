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

#include "inventory.h"
#include "game_character.h"
#include "game_item.h"
#include "proto_data/project.h"
#include "common/linear_set.h"
#include "binary_io/vector_sink.h"
#include "binary_io/writer.h"
#include "binary_io/reader.h"
#include "wowpp_protocol/wowpp_world_realm.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	io::Writer & operator<<(io::Writer &w, ItemData const& object)
	{
		w.writePOD(object);
		return w;
	}

	io::Reader & operator>>(io::Reader &r, ItemData& object)
	{
		r.readPOD(object);
		return r;
	}


	Inventory::Inventory(GameCharacter & owner)
		: m_owner(owner)
		, m_freeSlots(player_inventory_pack_slots::End - player_inventory_pack_slots::Start)	// Default slot count with only a backpack
	{
		// Warning: m_owner might not be completely constructed at this point
	}
	game::InventoryChangeFailure Inventory::createItems(const proto::ItemEntry & entry, UInt16 amount /* = 1*/, std::map<UInt16, UInt16> *out_addedBySlot /* = nullptr*/)
	{
		// Incorrect value used, so give at least one item
		if (amount == 0) amount = 1;

		// Limit the total amount of items
		const UInt16 itemCount = getItemCount(entry.id());
		if (entry.maxcount() > 0 &&
			itemCount + amount > entry.maxcount())
		{
			return game::inventory_change_failure::CantCarryMoreOfThis;
		}

		// Quick check if there are enough free slots (only works if we don't have an item of this type yet)
		const UInt16 requiredSlots = (amount - 1) / entry.maxstack() + 1;
		if ((itemCount == 0 || entry.maxstack() <= 1) &&
			requiredSlots > m_freeSlots)
		{
			return game::inventory_change_failure::InventoryFull;
		}

		// We need to remember free slots, since we first want to stack items up as best as possible
		LinearSet<UInt16> emptySlots;
		// We also need to remember all valid slots, that contain an item of that entry but are not 
		// at the stack limit so we can fill up those stacks.
		LinearSet<UInt16> usedCapableSlots;
		// This counter represent the number of available space for this item in total
		UInt16 availableStacks = 0;

		// This variable is used so that we can take a shortcut. Since we know the total amount of
		// this item entry in the inventory, we can determine whether we have found all items
		UInt16 itemsProcessed = 0;

		// Now do the iteration. First check the main bag
		// TODO: Check bags, too
		for (UInt8 slot = player_inventory_pack_slots::Start; slot < player_inventory_pack_slots::End; ++slot)
		{
			const UInt16 absoluteSlot = getAbsoluteSlot(player_inventory_slots::Bag_0, slot);
			
			// Check if this slot is empty
			auto it = m_itemsBySlot.find(absoluteSlot);
			if (it == m_itemsBySlot.end())
			{
				// Increase counter
				availableStacks += entry.maxstack();

				// Remember this slot for later and skip it for now
				emptySlots.add(absoluteSlot);
				if (itemsProcessed >= itemCount &&
					emptySlots.size() >= requiredSlots)
				{
					break;
				}

				// If we processed all items, we want to make sure, that we found enough free slots as well
				continue;
			}

			// It is not empty, so check if the item is of the same entry
			if (it->second->getEntry().id() != entry.id())
			{
				// Different item
				continue;
			}

			// Get the items stack count
			const UInt32 stackCount = it->second->getStackCount();
			itemsProcessed += stackCount;
			
			// Check if the item's stack limit is reached
			if (stackCount >= entry.maxstack())
			{
				if (itemsProcessed >= itemCount &&
					emptySlots.size() >= requiredSlots)
				{
					break;
				}

				// If we processed all items, we want to make sure, that we found enough free slots as well
				continue;
			}

			// Stack limit not reached, remember this slot
			availableStacks += (entry.maxstack() - stackCount);
			usedCapableSlots.add(absoluteSlot);
		}

		// Now we can determine if there is enough space
		if (amount > availableStacks)
		{
			// Not enough space
			return game::inventory_change_failure::InventoryFull;
		}

		// Now finally create the items. First, fill up all used stacks
		UInt16 amountLeft = amount;
		for (auto &slot : usedCapableSlots)
		{
			auto item = m_itemsBySlot[slot];

			// Added can not be greater than amountLeft, so we don't need a check on subtraction
			UInt16 added = item->addStacks(amountLeft);
			amountLeft -= added;

			// Notify update
			if (added > 0)
			{
				// Increase cached counter
				m_itemCounter[entry.id()] += added;

				if (out_addedBySlot) out_addedBySlot->insert(std::make_pair(slot, added));

				// Notify update
				itemInstanceUpdated(item, slot);
				m_owner.forceFieldUpdate(character_fields::InvSlotHead + ((slot & 0xFF) * 2));
				m_owner.forceFieldUpdate(character_fields::InvSlotHead + ((slot & 0xFF) * 2) + 1);
			}

			// Everything added
			if (amountLeft == 0)
				break;
		}

		// Are there still items left?
		if (amountLeft)
		{
			// Now iterate through all empty slots
			for (auto &slot : emptySlots)
			{
				// Create a new item instance
				auto item = std::make_shared<GameItem>(m_owner.getProject(), entry);
				item->initialize();

				// We need a valid world instance for this
				auto *world = m_owner.getWorldInstance();
				assert(world);

				// Determine slot
				UInt8 bag = 0, subslot = 0;
				getRelativeSlots(slot, bag, subslot);

				// Generate a new id for this item based on the characters world instance
				auto newItemId = world->getItemIdGenerator().generateId();
				item->setGuid(createEntryGUID(newItemId, entry.id(), wowpp::guid_type::Item));
				item->setUInt64Value(item_fields::Contained, m_owner.getGuid());
				item->setUInt64Value(item_fields::Owner, m_owner.getGuid());
				
				// One stack has been created by initializing the item
				amountLeft--;

				// Modify stack count
				UInt16 added = item->addStacks(amountLeft);
				amountLeft -= added;

				// Increase cached counter
				m_itemCounter[entry.id()] += added + 1;
				if (out_addedBySlot) out_addedBySlot->insert(std::make_pair(slot, added + 1));

				// Add this item to the inventory slot and reduce our free slot cache
				m_itemsBySlot[slot] = item;
				m_freeSlots--;

				// Notify creation
				if (bag == player_inventory_slots::Bag_0)
				{
					m_owner.setUInt64Value(character_fields::InvSlotHead + (subslot * 2), item->getGuid());
					if (isEquipmentSlot(slot))
					{
						m_owner.setUInt32Value(character_fields::VisibleItem1_0 + (subslot * 16), item->getEntry().id());
						m_owner.setUInt64Value(character_fields::VisibleItem1_CREATOR + (subslot * 16), item->getUInt64Value(item_fields::Creator));
					}
				}

				itemInstanceCreated(item, slot);

				// All done
				if (amountLeft == 0)
					break;
			}
		}

		// WARNING: There should never be any items left here!
		assert(amountLeft == 0);
		if (amountLeft > 0)
		{
			ELOG("Could not add all items, something went really wrong! " << __FUNCTION__);
			return game::inventory_change_failure::InventoryFull;
		}

		// Quest check
		m_owner.onQuestItemAddedCredit(entry, amount);

		// Everything okay
		return game::inventory_change_failure::Okay;
	}
	game::InventoryChangeFailure Inventory::removeItems(const proto::ItemEntry & entry, UInt16 amount)
	{
		// If amount equals 0, remove ALL items of that entry.
		const UInt16 itemCount = getItemCount(entry.id());
		if (amount == 0) amount = itemCount;

		// We don't have enough items, so we don't need to bother iterating through
		if (itemCount < amount)
		{
			// Maybe use a different result
			return game::inventory_change_failure::ItemNotFound;
		}

		// Counter used to know when to stop iteration
		UInt16 itemsToDelete = amount;

		// Now do the iteration. First check the main bag
		// TODO: Check bags, too
		for (UInt8 slot = player_inventory_pack_slots::Start; slot < player_inventory_pack_slots::End; ++slot)
		{
			const UInt16 absoluteSlot = getAbsoluteSlot(player_inventory_slots::Bag_0, slot);

			// Check if this slot is empty
			auto it = m_itemsBySlot.find(absoluteSlot);
			if (it == m_itemsBySlot.end())
			{
				// Empty slot
				continue;
			}

			// It is not empty, so check if the item is of the same entry
			if (it->second->getEntry().id() != entry.id())
			{
				// Different item
				continue;
			}

			// Get the items stack count
			const UInt32 stackCount = it->second->getStackCount();
			if (stackCount <= itemsToDelete)
			{
				// Remove item at this slot
				auto result = removeItem(absoluteSlot);
				if (result != game::inventory_change_failure::Okay)
				{
					ELOG("Could not remove item at slot " << absoluteSlot);
				}
				else
				{
					// Reduce counter
					itemsToDelete -= stackCount;
				}
			}
			else
			{
				// Reduce stack count
				it->second->setUInt32Value(item_fields::StackCount, stackCount - itemsToDelete);
				m_itemCounter[entry.id()] -= (stackCount - itemsToDelete);
				itemsToDelete = 0;

				// Notify client about this update
				itemInstanceUpdated(it->second, slot);
			}

			// All items processed, we can stop here
			if (itemsToDelete == 0)
				break;
		}

		// WARNING: There should never be any items left here!
		assert(itemsToDelete == 0);
		if (itemsToDelete > 0)
		{
			ELOG("Could not remove all items, something went really wrong! " << __FUNCTION__);
		}

		return game::inventory_change_failure::Okay;
	}
	game::InventoryChangeFailure Inventory::removeItem(UInt16 absoluteSlot, UInt16 stacks/* = 0*/)
	{
		// Try to find item
		auto it = m_itemsBySlot.find(absoluteSlot);
		if (it == m_itemsBySlot.end())
		{
			return game::inventory_change_failure::ItemNotFound;
		}

		// Updated cached item counter
		const UInt32 stackCount = it->second->getStackCount();
		if (stacks == 0 || stacks > stackCount) stacks = stackCount;
		m_itemCounter[it->second->getEntry().id()] -= stacks;

		if (stackCount == stacks)
		{
			// Remove item from slot
			auto item = it->second;
			m_itemsBySlot.erase(it);
			m_freeSlots++;

			UInt8 bag = 0, subslot = 0;
			getRelativeSlots(absoluteSlot, bag, subslot);
			if (bag == player_inventory_slots::Bag_0)
			{
				m_owner.setUInt64Value(character_fields::InvSlotHead + (subslot * 2), 0);
				if (isEquipmentSlot(absoluteSlot))
				{
					m_owner.setUInt32Value(character_fields::VisibleItem1_0 + (subslot * 16), item->getEntry().id());
					m_owner.setUInt64Value(character_fields::VisibleItem1_CREATOR + (subslot * 16), item->getUInt64Value(item_fields::Creator));
				}
			}

			// Notify about destruction
			itemInstanceDestroyed(item, absoluteSlot);
		}
		else
		{
			it->second->setUInt32Value(item_fields::StackCount, stackCount - stacks);
			itemInstanceUpdated(it->second, absoluteSlot);
		}

		// Quest check
		m_owner.onQuestItemRemovedCredit(it->second->getEntry(), stacks);

		return game::inventory_change_failure::Okay;
	}
	game::InventoryChangeFailure Inventory::swapItems(UInt16 slotA, UInt16 slotB)
	{
		// We need a valid source slot
		auto srcItem = getItemAtSlot(slotA);
		auto dstItem = getItemAtSlot(slotB);
		if (!srcItem)
		{
			m_owner.inventoryChangeFailure(game::inventory_change_failure::ItemNotFound, srcItem.get(), dstItem.get());
			return game::inventory_change_failure::ItemNotFound;
		}

		// Owner has to be alive
		if (!m_owner.isAlive())
		{
			m_owner.inventoryChangeFailure(game::inventory_change_failure::YouAreDead, srcItem.get(), dstItem.get());
			return game::inventory_change_failure::YouAreDead;
		}

		// Verify destination slot for source item
		auto result = isValidSlot(slotB, srcItem->getEntry());
		if (result != game::inventory_change_failure::Okay)
		{
			m_owner.inventoryChangeFailure(result, srcItem.get(), dstItem.get());
			return result;
		}

		// If there is an item in the destination slot, also verify the source slot
		if (dstItem)
		{
			result = isValidSlot(slotA, dstItem->getEntry());
			if (result != game::inventory_change_failure::Okay)
			{
				m_owner.inventoryChangeFailure(result, srcItem.get(), dstItem.get());
				return result;
			}
		}

		// Everything seems to be okay, swap items
		m_owner.setUInt64Value(character_fields::InvSlotHead + (slotA & 0xFF) * 2, (dstItem ? dstItem->getGuid() : 0));
		m_owner.setUInt64Value(character_fields::InvSlotHead + (slotB & 0xFF) * 2, srcItem->getGuid());
		std::swap(m_itemsBySlot[slotA], m_itemsBySlot[slotB]);
		if (!dstItem)
		{
			// Remove new item in SlotA
			m_itemsBySlot.erase(slotA);

			// No item in slot A, and slot A was an inventory slot, so this gives us another free slot
			if (isInventorySlot(slotA))
				m_freeSlots++;
			// No item in slot A, and slot B is an inventory slot, so this will use another free slot
			else if (isInventorySlot(slotB))
			{
				assert(m_freeSlots >= 1);
				m_freeSlots--;
			}
		}

		// Update visuals
		if (isEquipmentSlot(slotA))
		{
			m_owner.setUInt32Value(character_fields::VisibleItem1_0 + ((slotA & 0xFF) * 16), (dstItem ? dstItem->getEntry().id() : 0));
			m_owner.setUInt64Value(character_fields::VisibleItem1_CREATOR + ((slotA & 0xFF) * 16), (dstItem ? dstItem->getUInt64Value(item_fields::Creator) : 0));
		}
		if (isEquipmentSlot(slotB))
		{
			m_owner.setUInt32Value(character_fields::VisibleItem1_0 + ((slotB & 0xFF) * 16), srcItem->getEntry().id());
			m_owner.setUInt64Value(character_fields::VisibleItem1_CREATOR + ((slotB & 0xFF) * 16), srcItem->getUInt64Value(item_fields::Creator));
		}

		return game::inventory_change_failure::Okay;
	}
	namespace
	{
		game::weapon_prof::Type weaponProficiency(UInt32 subclass)
		{
			switch (subclass)
			{
				case game::item_subclass_weapon::Axe:
					return game::weapon_prof::OneHandAxe;
				case game::item_subclass_weapon::Axe2:
					return game::weapon_prof::TwoHandAxe;
				case game::item_subclass_weapon::Bow:
					return game::weapon_prof::Bow;
				case game::item_subclass_weapon::CrossBow:
					return game::weapon_prof::Crossbow;
				case game::item_subclass_weapon::Dagger:
					return game::weapon_prof::Dagger;
				case game::item_subclass_weapon::Fist:
					return game::weapon_prof::Fist;
				case game::item_subclass_weapon::Gun:
					return game::weapon_prof::Gun;
				case game::item_subclass_weapon::Mace:
					return game::weapon_prof::OneHandMace;
				case game::item_subclass_weapon::Mace2:
					return game::weapon_prof::TwoHandMace;
				case game::item_subclass_weapon::Polearm:
					return game::weapon_prof::Polearm;
				case game::item_subclass_weapon::Staff:
					return game::weapon_prof::Staff;
				case game::item_subclass_weapon::Sword:
					return game::weapon_prof::OneHandSword;
				case game::item_subclass_weapon::Sword2:
					return game::weapon_prof::TwoHandSword;
				case game::item_subclass_weapon::Thrown:
					return game::weapon_prof::Throw;
				case game::item_subclass_weapon::Wand:
					return game::weapon_prof::Wand;
			}

			return game::weapon_prof::None;
		}
		game::armor_prof::Type armorProficiency(UInt32 subclass)
		{
			switch (subclass)
			{
				case game::item_subclass_armor::Misc:
					return game::armor_prof::Common;
				case game::item_subclass_armor::Buckler:
				case game::item_subclass_armor::Shield:
					return game::armor_prof::Shield;
				case game::item_subclass_armor::Cloth:
					return game::armor_prof::Cloth;
				case game::item_subclass_armor::Leather:
					return game::armor_prof::Leather;
				case game::item_subclass_armor::Mail:
					return game::armor_prof::Mail;
				case game::item_subclass_armor::Plate:
					return game::armor_prof::Plate;
			}

			return game::armor_prof::None;
		}
	}
	game::InventoryChangeFailure Inventory::isValidSlot(UInt16 slot, const proto::ItemEntry & entry) const
	{
		// Split the absolute slot
		UInt8 bag = 0, subslot = 0;
		if (!getRelativeSlots(slot, bag, subslot))
			return game::inventory_change_failure::InternalBagError;

		// Check if it is a special bag....
		if (isEquipmentSlot(slot))
		{
			auto armorProf = m_owner.getArmorProficiency();
			auto weaponProf = m_owner.getWeaponProficiency();

			// Determine whether the provided inventory type can go into the slot
			if (entry.itemclass() == game::item_class::Weapon)
			{
				if ((weaponProf & weaponProficiency(entry.subclass())) == 0)
				{
					return game::inventory_change_failure::NoRequiredProficiency;
				}
			}
			else if (entry.itemclass() == game::item_class::Armor)
			{
				if ((armorProf & armorProficiency(entry.subclass())) == 0)
				{
					return game::inventory_change_failure::NoRequiredProficiency;
				}
			}

			if (entry.requiredlevel() > 0 &&
				entry.requiredlevel() > m_owner.getLevel())
			{
				return game::inventory_change_failure::CantEquipLevel;
			}

			if (entry.requiredskill() != 0 &&
				!m_owner.hasSkill(entry.requiredskill()))
			{
				return game::inventory_change_failure::CantEquipSkill;
			}

			// Validate equipment slots
			auto srcInvType = entry.inventorytype();
			switch (subslot)
			{
				case player_equipment_slots::Head:
					if (srcInvType != game::inventory_type::Head)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Body:
					if (srcInvType != game::inventory_type::Body)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Chest:
					if (srcInvType != game::inventory_type::Chest &&
						srcInvType != game::inventory_type::Robe)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Feet:
					if (srcInvType != game::inventory_type::Feet)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Finger1:
				case player_equipment_slots::Finger2:
					if (srcInvType != game::inventory_type::Finger)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Trinket1:
				case player_equipment_slots::Trinket2:
					if (srcInvType != game::inventory_type::Trinket)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Hands:
					if (srcInvType != game::inventory_type::Hands)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Legs:
					if (srcInvType != game::inventory_type::Legs)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Mainhand:
					if (srcInvType != game::inventory_type::MainHandWeapon &&
						srcInvType != game::inventory_type::TwoHandedWeapon &&
						srcInvType != game::inventory_type::Weapon)
					{
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					}
					else if (srcInvType == game::inventory_type::TwoHandedWeapon)
					{
						auto offhand = getItemAtSlot(getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Offhand));
						if (offhand)
						{
							// We need to be able to store the offhand weapon in the inventory
							auto result = canStoreItems(offhand->getEntry());
							if (result != game::inventory_change_failure::Okay)
							{
								return result;
							}
						}
					}
					break;
				case player_equipment_slots::Offhand:
					if (srcInvType != game::inventory_type::OffHandWeapon &&
						srcInvType != game::inventory_type::Shield &&
						srcInvType != game::inventory_type::Weapon)
					{
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					}
					else
					{
						if (srcInvType != game::inventory_type::Shield &&
							!m_owner.canDualWield())
						{
							return game::inventory_change_failure::CantDualWield;
						}

						auto item = getItemAtSlot(getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Mainhand));
						if (item &&
							item->getEntry().inventorytype() == game::inventory_type::TwoHandedWeapon)
						{
							return game::inventory_change_failure::CantEquipWithTwoHanded;
						}
					}
					break;
				case player_equipment_slots::Ranged:
					if (srcInvType != game::inventory_type::Ranged)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Shoulders:
					if (srcInvType != game::inventory_type::Shoulders)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Tabard:
					if (srcInvType != game::inventory_type::Tabard)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Waist:
					if (srcInvType != game::inventory_type::Waist)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				case player_equipment_slots::Wrists:
					if (srcInvType != game::inventory_type::Wrists)
						return game::inventory_change_failure::ItemDoesNotGoToSlot;
					break;
				default:
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
			}
		}
		else if (isInventorySlot(slot))
		{
			// TODO: Inventory slot check
		}
		else if (isBagSlot(slot))
		{
			// TODO: Validate bag
		}

		return game::inventory_change_failure::Okay;
	}
	game::InventoryChangeFailure Inventory::canStoreItems(const proto::ItemEntry & entry, UInt16 amount) const
	{
		// Incorrect value used, so give at least one item
		if (amount == 0) amount = 1;

		// Limit the total amount of items
		const UInt16 itemCount = getItemCount(entry.id());
		if (entry.maxcount() > 0 &&
			itemCount + amount > entry.maxcount())
		{
			return game::inventory_change_failure::CantCarryMoreOfThis;
		}

		// Quick check if there are enough free slots (only works if we don't have an item of this type yet)
		const UInt16 requiredSlots = (amount - 1) / entry.maxstack() + 1;
		if ((itemCount == 0 || entry.maxstack() <= 1) &&
			requiredSlots > m_freeSlots)
		{
			return game::inventory_change_failure::InventoryFull;
		}

		// TODO

		return game::inventory_change_failure::Okay;
	}
	UInt16 Inventory::getItemCount(UInt32 itemId) const
	{
		auto it = m_itemCounter.find(itemId);
		return (it != m_itemCounter.end() ? it->second : 0);
	}
	UInt16 Inventory::getAbsoluteSlot(UInt8 bag, UInt8 slot) 
	{
		return (bag << 8) | slot;
	}
	bool Inventory::getRelativeSlots(UInt16 absoluteSlot, UInt8 & out_bag, UInt8 & out_slot) 
	{
		out_bag = static_cast<UInt8>(absoluteSlot >> 8);
		out_slot = static_cast<UInt8>(absoluteSlot & 0xFF);
		return true;
	}
	std::shared_ptr<GameItem> Inventory::getItemAtSlot(UInt16 absoluteSlot) const
	{
		auto it = m_itemsBySlot.find(absoluteSlot);
		if (it != m_itemsBySlot.end()) return it->second;

		return std::shared_ptr<GameItem>();
	}
	bool Inventory::findItemByGUID(UInt64 guid, UInt16 & out_slot) const
	{
		for (auto &item : m_itemsBySlot)
		{
			if (item.second->getGuid() == guid)
			{
				out_slot = item.first;
				return true;
			}
		}

		return false;
	}
	bool Inventory::isEquipmentSlot(UInt16 absoluteSlot)
	{
		return (
			absoluteSlot >> 8 == player_inventory_slots::Bag_0 &&
			(absoluteSlot & 0xFF) < player_equipment_slots::End
			);
	}
	bool Inventory::isBagPackSlot(UInt16 absoluteSlot)
	{
		return (
			absoluteSlot >> 8 >= player_inventory_slots::Start &&
			absoluteSlot >> 8 < player_inventory_slots::End
			);
	}
	bool Inventory::isInventorySlot(UInt16 absoluteSlot)
	{
		return (
			absoluteSlot >> 8 == player_inventory_slots::Bag_0 &&
			(absoluteSlot & 0xFF) >= player_inventory_pack_slots::Start &&
			(absoluteSlot & 0xFF) < player_inventory_pack_slots::End
			);
	}
	bool Inventory::isBagSlot(UInt16 absoluteSlot)
	{
		return (absoluteSlot >> 8 != player_inventory_slots::Bag_0);
	}
	void Inventory::addRealmData(const ItemData & data)
	{
		m_realmData.push_back(data);
	}
	void Inventory::addSpawnBlocks(std::vector<std::vector<char>>& out_blocks)
	{
		// Reconstruct realm data if available
		if (!m_realmData.empty())
		{
			// World instance has to be ready
			auto *world = m_owner.getWorldInstance();
			if (!world)
			{
				return;
			}

			// Iterate through all entries
			for (auto &data : m_realmData)
			{
				auto *entry = m_owner.getProject().items.getById(data.entry);
				if (!entry)
				{
					ELOG("Could not find item " << data.entry);
					continue;
				}

				// Create a new item instance
				auto item = std::make_shared<GameItem>(m_owner.getProject(), *entry);
				item->initialize();
				item->setUInt64Value(item_fields::Owner, m_owner.getGuid());
				item->setUInt64Value(item_fields::Creator, data.creator);
				item->setUInt64Value(item_fields::Contained, data.contained);
				item->setUInt32Value(item_fields::Durability, data.durability);
				
				// Determine slot
				UInt8 bag = 0, subslot = 0;
				getRelativeSlots(data.slot, bag, subslot);

				// Generate a new id for this item based on the characters world instance
				auto newItemId = world->getItemIdGenerator().generateId();
				item->setGuid(createEntryGUID(newItemId, entry->id(), wowpp::guid_type::Item));
				if (bag == player_inventory_slots::Bag_0)
				{
					m_owner.setUInt64Value(character_fields::InvSlotHead + (subslot * 2), item->getGuid());
					if (isEquipmentSlot(data.slot))
					{
						m_owner.setUInt32Value(character_fields::VisibleItem1_0 + (subslot * 16), item->getEntry().id());
						m_owner.setUInt64Value(character_fields::VisibleItem1_CREATOR + (subslot * 16), item->getUInt64Value(item_fields::Creator));
					}
				}

				// Modify stack count
				item->addStacks(data.stackCount - 1);
				m_itemCounter[data.entry] += data.stackCount;

				// Add this item to the inventory slot and reduce our free slot cache
				m_itemsBySlot[data.slot] = std::move(item);
				
				// Inventory slot used
				if (isInventorySlot(data.slot))
					m_freeSlots--;
			}

			// Clear realm data since we don't need it any more
			m_realmData.clear();
		}

		for (auto &pair : m_itemsBySlot)
		{
			std::vector<char> createItemBlock;
			io::VectorSink createItemSink(createItemBlock);
			io::Writer createItemWriter(createItemSink);
			{
				UInt8 updateType = 0x02;						// Item
				UInt8 updateFlags = 0x08 | 0x10;				// 
				UInt8 objectTypeId = 0x01;						// Item
				UInt64 guid = pair.second->getGuid();

				// Header with object guid and type
				createItemWriter
					<< io::write<NetUInt8>(updateType);
				UInt64 guidCopy = guid;
				UInt8 packGUID[8 + 1];
				packGUID[0] = 0;
				size_t size = 1;
				for (UInt8 i = 0; guidCopy != 0; ++i)
				{
					if (guidCopy & 0xFF)
					{
						packGUID[0] |= UInt8(1 << i);
						packGUID[size] = UInt8(guidCopy & 0xFF);
						++size;
					}

					guidCopy >>= 8;
				}
				createItemWriter.sink().write((const char*)&packGUID[0], size);
				createItemWriter
					<< io::write<NetUInt8>(objectTypeId)
					<< io::write<NetUInt8>(updateFlags);
				if (updateFlags & 0x08)
				{
					createItemWriter
						<< io::write<NetUInt32>(guidLowerPart(guid));
				}
				if (updateFlags & 0x10)
				{
					createItemWriter
						<< io::write<NetUInt32>((guid << 48) & 0x0000FFFF);
				}

				pair.second->writeValueUpdateBlock(createItemWriter, m_owner, true);
			}
			out_blocks.emplace_back(std::move(createItemBlock));
		}
	}

	io::Writer &operator << (io::Writer &w, Inventory const& object)
	{
		if (object.m_realmData.empty())
		{
			// Inventory has actual item instances, so we serialize this object for realm usage
			w
				<< io::write<NetUInt16>(object.m_itemsBySlot.size());
			for (const auto &pair : object.m_itemsBySlot)
			{
				ItemData data;
				data.entry = pair.second->getEntry().id();
				data.slot = pair.first;
				data.stackCount = pair.second->getStackCount();
				data.creator = pair.second->getUInt64Value(item_fields::Creator);
				data.contained = pair.second->getUInt64Value(item_fields::Contained);
				data.durability = pair.second->getUInt32Value(item_fields::Durability);
				data.randomPropertyIndex = 0;
				data.randomSuffixIndex = 0;
				w
					<< data;
			}
		}
		else
		{
			// Inventory has realm data left, and no item instances
			w
				<< io::write<NetUInt16>(object.m_realmData.size());
			for (const auto &data : object.m_realmData)
			{
				w
					<< data;
			}
		}

		return w;
	}
	io::Reader &operator >> (io::Reader &r, Inventory& object)
	{
		object.m_itemsBySlot.clear();
		object.m_freeSlots = player_inventory_pack_slots::End - player_inventory_pack_slots::Start;
		object.m_itemCounter.clear();
		object.m_realmData.clear();

		// Read amount of items
		UInt16 itemCount = 0;
		r >> io::read<NetUInt16>(itemCount);

		// Read realm data
		object.m_realmData.resize(itemCount);
		for (UInt16 i = 0; i < itemCount; ++i)
		{
			r >> object.m_realmData[i];
		}

		return r;
	}
}