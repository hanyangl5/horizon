/*****************************************************************/ /**
 * \file   Container.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <array>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../memory/Memory.h"

namespace Horizon::Container {

using String = std::pmr::string;
template <typename T, size_t size> using FixedArray = std::array<T, size>;
template <typename T> using Array = std::pmr::vector<T>;
template <typename T> using Set = std::pmr::set<T>;
template <typename T> using HashSet = std::pmr::unordered_set<T>;
template <typename Key, typename Val> using Map = std::pmr::map<Key, Val>;
template <typename Key, typename Val> using HashMap = std::pmr::unordered_map<Key, Val>;

} // namespace Horizon::Container