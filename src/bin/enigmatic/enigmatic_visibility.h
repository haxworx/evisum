#ifndef ENIGMATIC_VISIBILITY_H
#define ENIGMATIC_VISIBILITY_H

#if defined(__GNUC__) || defined(__clang__)
# define ENIGMATIC_API __attribute__((visibility("default")))
#else
# define ENIGMATIC_API
#endif

#endif
