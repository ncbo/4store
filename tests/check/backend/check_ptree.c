#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include "check_backend.h"

#include "../../../src/backend/ptree.h"
#include "../../../src/backend/ptable.h"

TCase *make_ptree_tc(void) {
  TCase *tc = tcase_create ("ptree");
  //tcase_add_unchecked_fixture (tc, setup, teardown);
  return tc;
}
