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

#ifdef _MSC_VER
#	pragma once
#endif

// C Runtime Library
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cmath>

// STL Libraries
#include <map>
#include <unordered_map>
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <random>
#include <functional>
#include <iomanip>
#include <queue>
#include <cctype>
#include <sstream>
#include <locale>
#include <istream>
#include <fstream>
#include <atomic>
#include <forward_list>
#include <initializer_list>
#include <list>
#include <iterator>
#include <exception>
#include <type_traits>
#include <thread>

// Boost Libraies
#include <boost/optional.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/asio.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/io/ios_state.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/uuid/sha1.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/type_traits/is_float.hpp>
#include <boost/variant.hpp>
#include <boost/spirit/include/classic.hpp>
#include <boost/any.hpp>
#include <boost/program_options.hpp>

#include "mysql_wrapper/include_mysql.h"

#include <Ogre.h>

#include "simple/simple.hpp"
