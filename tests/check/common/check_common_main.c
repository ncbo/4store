#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "check_common.h"

int main (void)
{
  setenv ("CK_RUN_SUITE", "common", 1);
  setenv ("CK_RUN_CASE", "datatypes", 1);

  Suite *test_suite = suite_create("common");

  suite_add_tcase (test_suite, make_datatypes_tc() );
  suite_add_tcase (test_suite, make_hash_tc() );

  SRunner *sr = srunner_create (test_suite);
  srunner_run_all (sr, CK_VERBOSE);
  int number_failed = srunner_ntests_failed (sr);
  srunner_free (sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
