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

#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>

namespace urd::repr
{

/// Elements in the first segment of a segmented container; later segments grow geometrically.
inline constexpr std::size_t default_first_segment_size = 32;

/// The same for a sequence of T: fewer (one at least) where T is large, so that the first segment takes about a page.
template<typename T>
inline constexpr std::size_t first_segment_size_of = std::bit_floor(std::clamp<std::size_t>(4096 / sizeof(T), 1, default_first_segment_size));

/// Arrays in the first segment of the bit-packed and block array pools.
inline constexpr std::size_t default_first_array_segment_size = 16;

/// Bytes in the first segment of byte storage (raw array and vector pools, segmented buffers).
inline constexpr std::size_t default_first_segment_bytes = 1024;

}  // namespace urd::repr
