#pragma once

// ThreadSanitizer is extremely slow on atomics-heavy (lock-free) code, so scale
// down stress-test sizes so the sanitizer CI job finishes in a reasonable time.
#if defined(__SANITIZE_THREAD__)
#define ZEST_TEST_ITERS(n) ((n) / 20 + 1)
#else
#define ZEST_TEST_ITERS(n) (n)
#endif
