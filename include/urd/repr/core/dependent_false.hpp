/*
 * Copyright (C) 2025-2026 Dominik Drexler
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

// Copied from yggdrasil (include/yggdrasil/core/dependent_false.hpp); names moved from ygg to urd::repr.
#ifndef URD_REPR_CORE_DEPENDENT_FALSE_HPP_
#define URD_REPR_CORE_DEPENDENT_FALSE_HPP_

#include <type_traits>

namespace urd::repr
{

template<typename T>
struct dependent_false : std::false_type
{
};

}

#endif
