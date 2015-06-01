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

#include "unit_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"

namespace wowpp
{
	UnitEntry::UnitEntry()
		: minLevel(1)
		, maxLevel(1)
		, maleModel(0)
		, femaleModel(0)
		, minLevelHealth(0)
		, maxLevelHealth(0)
		, minLevelMana(0)
		, maxLevelMana(0)
		, minMeleeDamage(0)
		, maxMeleeDamage(0)
		, minRangedDamage(0)
		, maxRangedDamage(0)
		, scale(1.0f)
		, allianceFactionID(0)
		, hordeFactionID(0)
		, family(0)
		, regeneratesHealth(true)
		, npcFlags(0)
		, unitFlags(0)
		, dynamicFlags(0)
		, extraFlags(0)
		, creatureTypeFlags(0)
		, walkSpeed(1.0f)
		, runSpeed(1.0f)
		, unitClass(1)
		, rank(0)
		, armor(0)
		, meleeBaseAttackTime(0)
		, rangedBaseAttackTime(0)
		, damageSchool(0)
		, minLootGold(0)
		, maxLootGold(0)
	{
		resistances.fill(0);
	}

	bool UnitEntry::load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

#define MIN_MAX_CHECK(minval, maxval) if (maxval < minval) maxval = minval; else if(minval > maxval) minval = maxval;

		wrapper.table.tryGetString("name", name);
		wrapper.table.tryGetString("subname", subname);
		wrapper.table.tryGetInteger("min_level", minLevel);
		wrapper.table.tryGetInteger("max_level", maxLevel);
		MIN_MAX_CHECK(minLevel, maxLevel);
		wrapper.table.tryGetInteger("model_id_1", maleModel);
		wrapper.table.tryGetInteger("model_id_2", femaleModel);
		wrapper.table.tryGetInteger("min_level_health", minLevelHealth);
		wrapper.table.tryGetInteger("max_level_health", maxLevelHealth);
		MIN_MAX_CHECK(minLevelHealth, maxLevelHealth);
		wrapper.table.tryGetInteger("min_level_mana", minLevelMana);
		wrapper.table.tryGetInteger("max_level_mana", maxLevelMana);
		MIN_MAX_CHECK(minLevelMana, maxLevelMana);
		wrapper.table.tryGetInteger("min_melee_dmg", minMeleeDamage);
		wrapper.table.tryGetInteger("max_melee_dmg", maxMeleeDamage);
		MIN_MAX_CHECK(minMeleeDamage, maxMeleeDamage);
		wrapper.table.tryGetInteger("min_ranged_dmg", minRangedDamage);
		wrapper.table.tryGetInteger("max_ranged_dmg", maxRangedDamage);
		MIN_MAX_CHECK(minRangedDamage, maxRangedDamage);
		wrapper.table.tryGetInteger("scale", scale);
		wrapper.table.tryGetInteger("rank", rank);
		wrapper.table.tryGetInteger("armor", armor);
		wrapper.table.tryGetInteger("melee_attack_time", meleeBaseAttackTime);
		wrapper.table.tryGetInteger("ranged_attack_time", rangedBaseAttackTime);
		wrapper.table.tryGetInteger("a_faction", allianceFactionID);
		wrapper.table.tryGetInteger("h_faction", hordeFactionID);
		
#undef MIN_MAX_CHECK

		return true;
	}

	void UnitEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) context.table.addKey("name", name);
		if (!subname.empty()) context.table.addKey("subname", subname);
		if (minLevel > 1) context.table.addKey("min_level", minLevel);
		if (maxLevel != minLevel) context.table.addKey("max_level", maxLevel);
		if (maleModel != 0) context.table.addKey("model_id_1", maleModel);
		if (femaleModel != 0) context.table.addKey("model_id_2", femaleModel);
		if (minLevelHealth != 0) context.table.addKey("min_level_health", minLevelHealth);
		if (maxLevelHealth != 0) context.table.addKey("max_level_health", maxLevelHealth);
		if (minLevelMana != 0) context.table.addKey("min_level_mana", minLevelMana);
		if (maxLevelMana != 0) context.table.addKey("max_level_mana", maxLevelMana);
		if (minMeleeDamage != 0) context.table.addKey("min_melee_dmg", minMeleeDamage);
		if (maxMeleeDamage != 0) context.table.addKey("max_melee_dmg", maxMeleeDamage);
		if (minRangedDamage != 0) context.table.addKey("min_ranged_dmg", minRangedDamage);
		if (maxRangedDamage != 0) context.table.addKey("max_ranged_dmg", maxRangedDamage);
		if (scale != 1.0f) context.table.addKey("scale", scale);
		if (rank != 0) context.table.addKey("rank", rank);
		if (armor != 0) context.table.addKey("armor", armor);
		if (meleeBaseAttackTime != 0) context.table.addKey("melee_attack_time", meleeBaseAttackTime);
		if (rangedBaseAttackTime != 0) context.table.addKey("ranged_attack_time", rangedBaseAttackTime);
		if (allianceFactionID != 0) context.table.addKey("a_faction", allianceFactionID);
		if (hordeFactionID != 0) context.table.addKey("h_faction", hordeFactionID);
	}
}