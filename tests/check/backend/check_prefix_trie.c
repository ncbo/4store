#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "check_backend.h"

#include "../../../src/common/4s-hash.h"
#include "../../../src/backend/prefix-trie.h"

void setup_hash_pt(void) {
    fs_hash_init(FS_HASH_UMAC);
}

void teardown_hash_pt(void) {
    fs_hash_fini();
}


START_TEST (check_prefixtrie_create)
{
  printf("trie prefix test\n");

}
END_TEST

TCase *make_prefix_trie_tc(void) {
    TCase *tc = tcase_create ("rhash");
    tcase_add_unchecked_fixture (tc, setup_hash_pt, teardown_hash_pt);
    tcase_add_test(tc, check_prefixtrie_create);
    tcase_set_timeout(tc, 0);
    return tc;
}


