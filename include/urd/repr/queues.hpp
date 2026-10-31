#pragma once

// Queues: elements reached by the order they leave in, with the standard adaptors' names (push, pop, front or top),
// and clear and reserve, which the adaptors lack, so that a queue reused run after run keeps its storage.
// One header per container.

#include "urd/repr/addressable_heap.hpp"
#include "urd/repr/bucket_queue.hpp"
#include "urd/repr/position_iterator.hpp"
#include "urd/repr/queue.hpp"
#include "urd/repr/ring_fifo.hpp"
