#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "check_backend.h"

#include "../../../src/backend/tbchain.h"
#include "../../../src/backend/mhash.h"

START_TEST (check_mhash_create)
{
  fs_mhash *hash = fs_mhash_open_filename("./tmp/check_test.mhash",
                                            O_RDWR | O_CREAT | O_TRUNC);
  fail_if(hash == NULL);
  fs_rid m = 0xD000000000001234;
  fs_index_node n = 0;
  int r = fs_mhash_put(hash,m, 0);
  fail_if(r != 0);
  r = fs_mhash_get(hash,m, &n);
  fail_if(r != 0 || n != 0);
  r = fs_mhash_put(hash,m, 12345);
  fail_if(r != 0);
  r = fs_mhash_get(hash,m, &n);
  fail_if(r != 0 || n != 12345);

  int i=0;
  printf("start asserting\n");
  int SLOTS = 1e4;
  srand ( time(NULL) );
  double t = fs_time();
  fs_rid checks[256+1];
  for (i=1; i <= SLOTS; i++) {
    fs_rid rid = (fs_rid) rand();
    r = fs_mhash_put(hash, rid, i);
    if (i <= 256)
      checks[i] = rid;

  }
  printf("done asserting %u in %.3f sec.\n",fs_mhash_count(hash), fs_time() - t);
  fs_rid_vector *keys = fs_mhash_get_keys(hash);
  fail_if(keys == NULL);
  fail_if(fs_rid_vector_length(keys) != fs_mhash_count(hash));
  fail_if(fs_rid_vector_length(keys) != SLOTS+1);
  for (i = 1; i < 256+1; i++) {
    fail_if(!fs_rid_vector_contains(keys, checks[i]));
    r = fs_mhash_get(hash, checks[i], &n);
    fail_if (r != 0 || n != i);
  }
  fs_mhash_close(hash);

}
END_TEST

TCase *make_mhash_tc(void) {
  TCase *tc = tcase_create ("mhash");
  //tcase_add_unchecked_fixture (tc, setup, teardown);
  tcase_add_test(tc, check_mhash_create);
  tcase_set_timeout(tc, 0);
  return tc;
}
