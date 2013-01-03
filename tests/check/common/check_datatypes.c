#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include "check_common.h"
#include "../../../src/common/4s-datatypes.h"

START_TEST (check_fs_rid_vector_add)
{
  fs_rid_vector *v = fs_rid_vector_new(0);
  fail_unless(v != NULL, "fs_rid_vector is NULL");
  int i=0;
  for (i=1000; i < 1e6; i++) {
      fs_rid_vector_append(v, i);
  }
  fail_if(fs_rid_vector_length(v) != (1e6 - 1000), "fs_rid_vector_length failed");
  for (i=1000; i < 1e2; i++)
      fail_if(!fs_rid_vector_contains(v, i),"Element '%d' not found in vector");
  for (i=765; i < 1e3; i++)
      fail_if(fs_rid_vector_contains(v, i),"Element '%d' found in vector.");
  fs_rid_vector_free(v);
}
END_TEST


START_TEST (check_fs_rid_vector_uniq)
{
  fs_rid_vector *v = fs_rid_vector_new(0);
  fail_unless(v != NULL, "fs_rid_vector is NULL");
  int i=0;
  for (i=100; i < 500; i++) {
      fs_rid_vector_append(v, i);
      fs_rid_vector_append(v, i);
  }
  fs_rid_vector_append(v,FS_RID_NULL);
  fail_if(fs_rid_vector_length(v) != (((500 - 100)*2) + 1), "fs_rid_vector_length failed");
  fs_rid_vector_uniq(v, 0); //Not remove nulls
  fail_if(fs_rid_vector_length(v) != ((500 - 100) + 1), "fs_rid_vector_length failed");
  for (i=1; i < fs_rid_vector_length(v) - 1; i++) {
      fail_if(v->data[i] == v->data[i-1], "unique values failed [%d %d]",v->data[i],v->data[i-1]);
  }
  fail_if(v->data[fs_rid_vector_length(v)-1] != FS_RID_NULL, "FS_RID_NULL isn't there.");
  v->data[123] = FS_RID_NULL;
  fs_rid_vector_uniq(v, 1); //remove nulls
  fail_if(fs_rid_vector_length(v) != ((500 - 100) -1), "fs_rid_vector_length failed");
  fail_if(v->data[fs_rid_vector_length(v)-1] == FS_RID_NULL, "FS_RID_NULL should not be there %llx",
          v->data[fs_rid_vector_length(v)-1]);
  fail_if(v->data[123] == FS_RID_NULL, "FS_RID_NULL should not be there %llx",
          v->data[fs_rid_vector_length(v)-1]);
  fs_rid_vector_free(v);
}
END_TEST


START_TEST (check_fs_rid_vector_sort)
{
  fs_rid_vector *v = fs_rid_vector_new(0);
  fail_unless(v != NULL, "fs_rid_vector is NULL");
  int i=0;
  for (i=100; i < 1e4; i++) {
      if (i % 2) {
          fs_rid_vector_append(v, i);
          fs_rid_vector_append(v, i+3);
          fs_rid_vector_append(v, i-10);
      } else {
          fs_rid_vector_append(v, i-1);
          fs_rid_vector_append(v, i+5);
          fs_rid_vector_append(v, i);
      }
  }
  fs_rid_vector_sort(v);
  fail_if(fs_rid_vector_length(v) != ((1e4 - 100) *3), "Length fail after sort");
  for (i=100; i < 1e4; i++) {
      fail_if ( !fs_rid_vector_contains(v, i), "Contain failed after sort");
  }
  for (i=0;i<fs_rid_vector_length(v) -1; i++) {
      fail_if (v->data[i] > v->data[i+1], "Sort does not match.");
  }
  fs_rid_vector_free(v);
}
END_TEST


START_TEST (check_fs_rid_vector_truncate)
{
  fs_rid_vector *v = fs_rid_vector_new(0);
  fail_unless(v != NULL, "fs_rid_vector is NULL");
  int i=0;
  for (i=100; i < 1e4; i++) {
      fs_rid_vector_append(v, i);
  }
  fs_rid_vector_truncate(v, 100);
  fail_if(fs_rid_vector_length(v) != 100);
  fail_if(v->data[fs_rid_vector_length(v)-1] != 199);
  fs_rid_vector_truncate(v, 1);
  fail_if(v->data[fs_rid_vector_length(v)-1] != 100);
  fs_rid_vector_free(v);
}
END_TEST


START_TEST (check_fs_rid_vector_grow)
{
  fs_rid_vector *v = fs_rid_vector_new(0);
  fail_unless(v != NULL, "fs_rid_vector is NULL");
  int i=0;
  for (i=100; i < 1e4; i++) {
      fs_rid_vector_append(v, i);
  }
  fail_if(fs_rid_vector_length(v) != (1e4 - 100));
  fs_rid_vector_grow(v, 2e4);
  fail_if(fs_rid_vector_length(v) != 2e4);
  for (i=0;i<2e4;i++) {
      v->data[i] = i;
  }
  fail_if(v->size != 2e4);
  fs_rid_vector_free(v);
}
END_TEST


START_TEST (check_fs_rid_set_add_contains)
{
  fs_rid_set *s = fs_rid_set_new();
  fail_unless(s != NULL);
  int i=0;
  for (i=0; i < 1e4; i++) {
      fs_rid_set_add(s, i);
  }
  for (i=0; i < 1e4; i++) {
      fail_if(!fs_rid_set_contains(s,i));
  }
  fs_rid_set_free(s);
}
END_TEST


START_TEST (check_fs_rid_set_scan)
{
  fs_rid_set *s = fs_rid_set_new();
  fail_unless(s != NULL);
  int i=0;
  int ELTS = 10000;
  for (i=1; i <= ELTS; i++) {
      fs_rid_set_add(s, (fs_rid) i);
      if ((i % 2 == 0) || (i % 5 == 0))
          fs_rid_set_add(s, (fs_rid) i);
  }
  int pass=0;
  for (pass=0; pass < 2; pass++) {
      fs_rid_set_rewind(s);
      fs_rid e = FS_RID_NULL;
      int count = 0;
      while((e = fs_rid_set_next(s)) != FS_RID_NULL) {
          count = count + 1;
          fail_if(e <= 0 || e > (ELTS));
      }
      fail_if(count != ELTS);
  }
  fs_rid_set_free(s);
}
END_TEST

TCase *make_datatypes_tc(void) {
  TCase *tc = tcase_create ("datatypes");
  /* fs_rid_vector */
  tcase_add_test (tc, check_fs_rid_vector_add);
  tcase_add_test (tc, check_fs_rid_vector_uniq);
  tcase_add_test (tc, check_fs_rid_vector_sort);
  tcase_add_test (tc, check_fs_rid_vector_truncate);
  tcase_add_test (tc, check_fs_rid_vector_grow);

  /* fs_rid_set */
  tcase_add_test (tc, check_fs_rid_set_add_contains);
  tcase_add_test (tc, check_fs_rid_set_scan);
  return tc;
}
