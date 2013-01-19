#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "check_backend.h"

int main (void)
{
  Suite *test_suite = suite_create("backend");

  /*
  suite_add_tcase (test_suite, make_tbchain_tc() );
  suite_add_tcase (test_suite, make_ptree_tc() );
  suite_add_tcase (test_suite, make_ptable_tc() );
  suite_add_tcase (test_suite, make_mhash_tc() );
  */
  suite_add_tcase (test_suite, make_rhash_tc() );

  SRunner *sr = srunner_create (test_suite);
  srunner_run_all (sr, CK_VERBOSE);
  int number_failed = srunner_ntests_failed (sr);
  srunner_free (sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
