#ifndef CHECK_COMMON_H
#define CHECK_COMMON_H

#ifndef TIMEOUT_TESTS_ENABLED
#define TIMEOUT_TESTS_ENABLED 0
#endif

TCase * make_hash_tc (void);
TCase * make_datatypes_tc (void);

#endif /* CHECK_COMMON_H */
