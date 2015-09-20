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

#include "program.h"
#include "log/default_log_levels.h"
#include "log/log_std_stream.h"
#include "common/crash_handler.h"
#include <boost/program_options.hpp>

using namespace std;

/// Procedural entry point of the application.
int main(int argc, char* argv[])
{
	namespace po = boost::program_options;

	// Add cout to the list of log output streams
	wowpp::g_DefaultLog.signal().connect(std::bind(
		wowpp::printLogEntry,
		std::ref(std::cout), std::placeholders::_1, wowpp::g_DefaultConsoleLogOptions));

	//constructor enables error handling
	wowpp::CrashHandler::get().enableDumpFile("WorldCrash.dmp");

	//when the application terminates unexpectedly
	const auto crashFlushConnection =
		wowpp::CrashHandler::get().onCrash.connect(
		[]()
	{
		ELOG("Application crashed...");
	});

	const std::string WorldServerDefaultConfig = "wowpp_world.cfg";
	std::string configFileName = WorldServerDefaultConfig;

	po::options_description desc("WoW++ world node, available options");
	desc.add_options()
		("help,h", "produce help message")
		("config,c", po::value<std::string>(&configFileName),
		("configuration file name, default: " + WorldServerDefaultConfig).c_str())
		;

	po::variables_map vm;
	try
	{
		po::store(
			po::command_line_parser(argc, argv).options(desc).run(),
			vm);
		po::notify(vm);
	}
	catch (const po::error &e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}

	if (vm.count("help"))
	{
		std::cerr << desc << '\n';
		return 0;
	}

	// Triggers if the program should be restarted
	bool shouldRestartProgram = false;

	do
	{
		// Run the main program
		wowpp::Program program;
		shouldRestartProgram = program.run(configFileName);
	} while (shouldRestartProgram);

	// Shutdown
	return 0;
}
