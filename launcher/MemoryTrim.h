#pragma once

/**
 * Keeps the launcher's memory small on Linux (glibc). Elsewhere these do nothing.
 *
 * configure() runs first thing in main(): two malloc arenas instead of one per thread, and big
 * blocks (images, network buffers) always come straight from the system, so freeing them gives
 * the memory back instead of leaving holes in the heap.
 * release() hands freed heap pages back to the system; call it when the launcher goes quiet.
 */
namespace MemoryTrim {
void configure();
void release();
}  // namespace MemoryTrim
