#pragma once

#include <cstdio>
#include <cstdlib>

/// Fails the test with the condition's text; active in every build type, unlike assert.
#define URD_CHECK(condition)                                                                           \
    do                                                                                                 \
    {                                                                                                  \
        if (!(condition))                                                                              \
        {                                                                                              \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);         \
            std::exit(EXIT_FAILURE);                                                                   \
        }                                                                                              \
    } while (false)
