#ifndef CIRCLE_MACROS_H
#define CIRCLE_MACROS_H

// Define macros that are defined and used in Circle's headers,
// but not in the newlib headers.

#define ASSERT_STATIC(expr) static_assert(expr, #expr)

#endif
