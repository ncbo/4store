#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include "check_common.h"

#include "../../../src/common/4s-hash.h"

void setup(void) {
    fs_hash_init(FS_HASH_UMAC);
}

void teardown(void) {
    fs_hash_fini();
}

START_TEST (check_fs_hash_uri)
{
    fs_rid rid = fs_hash_uri("http://foo.com/ba");
    fail_if(FS_IS_BNODE(rid));
    fail_if(FS_IS_LITERAL(rid));
    fail_if(!FS_IS_URI(rid));
    fail_if(rid == FS_RID_NULL);;
}
END_TEST

START_TEST (check_fs_hash_literal)
{
    fs_rid type = fs_hash_uri("string");
    fs_rid rid = fs_hash_literal("literal",type);
    fail_if(FS_IS_BNODE(rid));
    fail_if(!FS_IS_LITERAL(rid));
    fail_if(FS_IS_URI(rid));
    fail_if(rid == FS_RID_NULL);;
}
END_TEST

START_TEST (check_fs_hash_bnode)
{
    fs_rid rid = fs_hash_uri("bnode:b000000000001");
    fail_if(!FS_IS_BNODE(rid));
    fail_if(FS_IS_LITERAL(rid));
    fail_if(FS_IS_URI(rid));
    fail_if(rid == FS_RID_NULL);
}
END_TEST

START_TEST (check_fs_hash_globals_built)
{
    struct fs_globals  g = fs_global_constants();
    fail_if(g.xsd_uint == FS_RID_NULL);
    fail_if(g.system_config == FS_RID_NULL);
}
END_TEST

TCase *make_hash_tc(void) {
  TCase *tc = tcase_create ("hash");
  tcase_add_unchecked_fixture (tc, setup, teardown);
  tcase_add_test (tc, check_fs_hash_uri);
  tcase_add_test (tc, check_fs_hash_literal);
  tcase_add_test (tc, check_fs_hash_bnode);
  tcase_add_test (tc, check_fs_hash_globals_built);
  return tc;
}
