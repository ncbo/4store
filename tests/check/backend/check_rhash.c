#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "check_backend.h"

#include "../../../src/common/4s-hash.h"
#include "../../../src/backend/rhash.h"

void setup(void) {
    fs_hash_init(FS_HASH_UMAC);
}

void teardown(void) {
    fs_hash_fini();
}

static void free_resource(fs_resource *res) {
    if (res) {
        if (res->lex) free(res->lex);
        free(res);
    }
}
static fs_resource *new_literal_resource(char *lex, char *attr) {
    fs_resource *res = calloc(1, sizeof(fs_resource));
    res->attr = attr ? fs_hash_literal(attr, FS_RID_NULL) : FS_RID_NULL;
    res->rid = fs_hash_literal(lex, res->attr);
    res->lex = strdup(lex);
    return res;
}

START_TEST (check_rhash_create)
{
    fs_rhash * rh = fs_rhash_open_filename("./tmp/check_test.rhash",
                                            O_RDWR | O_CREAT | O_TRUNC);
    fail_if(rh == NULL);

    fs_resource *res = new_literal_resource("literal 1", "string");
    fs_rid res_key = res->rid;
    fs_rid attr_rid = res->attr;
    fs_rhash_put(rh,res);
    free(res);

    fs_rhash_close(rh);

    rh = fs_rhash_open_filename("./tmp/check_test.rhash",O_RDWR);

    fail_if(rh == NULL);
    fs_resource res_get;
    res_get.rid = res_key;
    res = new_literal_resource("literal 1", "string");
    int e = fs_rhash_get(rh, &res_get);
    fail_if(e != 0);
    fail_if(strcmp(res->lex, "literal 1") != 0);
    fail_if(attr_rid != res_get.attr);
    printf("returned .... %s\n",res->lex);
}
END_TEST

TCase *make_rhash_tc(void) {
    TCase *tc = tcase_create ("rhash");
    tcase_add_unchecked_fixture (tc, setup, teardown);
    tcase_add_test(tc, check_rhash_create);
    tcase_set_timeout(tc, 0);
    return tc;
}

