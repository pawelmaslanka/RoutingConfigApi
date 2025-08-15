/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */

#pragma once

// Headers arranged in alphabetical order
#include <forward_list>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

/** Provides aliases for types from C++ Standard Library */
namespace StdLib {
// Aliases arranged in alphabetical order
using Exception = std::exception;
using IFStream = std::ifstream;
using Mutex = std::mutex;
using OFStream = std::ofstream;
using OStrStream = std::ostringstream;
using String = std::string;
using StringView = std::string_view;
using Thread = std::thread;

template<class T> using ForwardList = std::forward_list<T>;
template<class T> using LockGuard = std::lock_guard<T>;
template<class T> using Optional = std::optional<T>;
template<class T> using SharedPtr = std::shared_ptr<T>;
template<class T> using Stack = std::stack<T>;
template<class T> using UniquePtr = std::unique_ptr<T>;
template<class T> using WeakPtr = std::weak_ptr<T>;
template<class T> using Vector = std::vector<T>;

template<class Key, class T> using Map = std::map<Key, T>;
template<class T1, class T2> using Pair = std::pair<T1, T2>;
} // namespace StdTypes
