#include "MemoryTrim.h"

#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace MemoryTrim {

void configure()
{
#if defined(__GLIBC__)
    mallopt(M_ARENA_MAX, 2);
    // A fixed threshold also turns off glibc's habit of raising it after big frees.
    mallopt(M_MMAP_THRESHOLD, 256 * 1024);
    mallopt(M_TRIM_THRESHOLD, 1024 * 1024);
#endif
}

void release()
{
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
}

}  // namespace MemoryTrim
