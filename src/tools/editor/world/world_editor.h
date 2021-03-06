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

#include "common/work_queue.h"
#include "page.h"
#include "loaded_page_section.h"
#include "page_neighborhood.h"
#include "page_loader_listener.h"
#include "page_pov_partitioner.h"
#include "world_page_loader.h"
#include "world_renderer.h"
#include "ogre_wrappers/scene_node_ptr.h"
#include <QMouseEvent>
#include "ogre_wrappers/qt_ogre_window.h"
#include "ogre_wrappers/entity_ptr.h"
#include "transform_widget.h"
#include "editor_application.h"
#include "ogre_wrappers/debug_drawer.h"
#include "ogre_wrappers/ogre_dbc_file_manager.h"

namespace Ogre
{
	class Camera;
    class Light;
}

namespace wowpp
{
	class Map;

	namespace proto
	{
		class Project;
		class MapEntry;
	}

	namespace editor
	{
		class WorldEditor final : public paging::IPageLoaderListener, public IScene
		{
		public:

			simple::signal<void(paging::PagePosition)> pageChanged;

		public:

			explicit WorldEditor(EditorApplication &app, Ogre::SceneManager &sceneMgr, Ogre::Camera &camera, proto::MapEntry &map, proto::Project &project);
			~WorldEditor();

			void update(float delta) override;
			void save();
			void onKeyPressed(const QKeyEvent *e) override;
			void onKeyReleased(const QKeyEvent *e) override;
			void onMousePressed(const QMouseEvent *e) override;
			void onMouseReleased(const QMouseEvent *e) override;
			void onMouseMoved(const QMouseEvent *e) override;
			void onDoubleClick(const QMouseEvent *e) override;
			void onSelection(Ogre::Entity &entity) override;
			void onSetPoint(const Ogre::Vector3 &point) override;
			void onAddUnitSpawn(wowpp::UInt32 entry, const Ogre::Vector3 &point) override;

		private:

			void onPageLoad(const paging::Page &page) override;
			void onPageAvailabilityChanged(const paging::PageNeighborhood &page, bool isAvailable) override;
			terrain::model::Page *getTerrainPage(terrain::model::PagePosition position);
			void onTransformToolChanged(TransformTool tool);

			void addUnitSpawn(proto::UnitSpawnEntry &entry, bool select = false);

		private:

			EditorApplication &m_app;
			Ogre::SceneManager &m_sceneMgr;
			Ogre::Camera &m_camera;
			proto::MapEntry &m_map;
			boost::asio::io_service m_dispatcher;
			boost::asio::io_service m_workQueue;
			std::unique_ptr<boost::asio::io_service::work> m_work;
			std::thread m_worker;
			std::unique_ptr<paging::LoadedPageSection> m_visibleSection;
			std::unique_ptr<WorldPageLoader> m_pageLoader;
			std::unique_ptr<paging::PagePOVPartitioner> m_memoryPointOfView;
			std::unique_ptr<view::WorldRenderer> m_worldRenderer;
			std::map<paging::PagePosition, terrain::model::Page> m_pages;
            Ogre::Light *m_light;
			proto::Project &m_project;
			std::vector<wowpp::ogre_utils::SceneNodePtr> m_spawnNodes;
			std::vector<wowpp::ogre_utils::EntityPtr> m_spawnEntities;
			std::unique_ptr<TransformWidget> m_transformWidget;
			simple::scoped_connection m_onTransformChanged;
			paging::PagePosition m_previousPage;
			std::unique_ptr<Map> m_mapInst;
			std::unique_ptr<OgreDebugDraw> m_debugDraw;
			simple::scoped_connection m_onShowNavMesh;
			Ogre::Vector3 m_start, m_target;
			bool m_startSet;
			ogre_utils::ManualObjectPtr m_pathObj;
			OgreDBCFilePtr m_displayDbc;
			OgreDBCFilePtr m_modelDbc;
			OgreDBCFilePtr m_objDisplayDbc;
			UInt32 m_nextUnitSpawn;
			std::map<UInt32, UInt32> m_wmoRefCount;
			std::map<UInt32, UInt32> m_doodadRefCount;
			std::map<UInt32, wowpp::ogre_utils::ManualObjectPtr> m_wmoGeometry;
			std::map<UInt32, wowpp::ogre_utils::ManualObjectPtr> m_doodadGeometry;
		};
	}
}
