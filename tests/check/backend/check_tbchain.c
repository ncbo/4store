#include <check.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include "check_backend.h"

#include "../../../src/common/timing.h"
#include "../../../src/backend/tbchain.h"
#include "../../../src/backend/mhash.h"

static double test_time()
{
    struct timeval now;

    gettimeofday(&now, 0);

    return (double)now.tv_sec + (now.tv_usec * 0.000001);
}

START_TEST (check_tbchain_create)
{

    int i;
    double begin = test_time();
    fs_tbchain *bc = fs_tbchain_open_filename("./tmp/check_test.tbchain",O_RDWR | O_CREAT | O_TRUNC);
    fail_if(bc == NULL);
    fs_index_node b = fs_tbchain_new_chain(bc);
    printf("b--> %u\n",b);
    fail_if(fs_tbchain_length(bc, b) > 0);
    fs_rid triple[3];
    triple[0] = 0xABC;
    triple[1] = 0xABC;
    triple[2] = 0xABC;
    b = fs_tbchain_add_triple(bc, b, triple);
    fail_if(b != 1023);
    fail_if(fs_tbchain_length(bc, b) != 1);

    fs_index_node c = fs_tbchain_new_chain(bc);
    fs_index_node d = fs_tbchain_new_chain(bc);
    for(i=0; i < 2e6; i++) {
        triple[0] = 0xCCC + i; triple[1] = 0xCCC + i;triple[2] = 0xCCC + i;
        c = fs_tbchain_add_triple(bc, c, triple);
        if (c<2) fail_if(1==1);
        if (i % 2) {
            triple[0] = 0xAAA + i; triple[1] = 0xAAA + i;triple[2] = 0xAAA + i;
            b = fs_tbchain_add_triple(bc, b, triple);
            if (b<2) fail_if(1==1);
        }
        if (i % 2) {
            triple[0] = 0xAAA + i; triple[1] = 0xAAA + i;triple[2] = 0xAAA + i;
            d = fs_tbchain_add_triple(bc, d, triple);
            if (d<2) fail_if(1==1);
        }
    }
    fail_if(fs_tbchain_length(bc, c) != 2e6);
    fs_tbchain_close(bc);
    TIME(ts_timing_report);
    double end = test_time();
    printf("TIMES: check_tbchain_create %.4f\n", (end - begin));
}
END_TEST


START_TEST (check_tbchain_iterate)
{

    int i;
    double begin = test_time();
    fs_tbchain *bc = fs_tbchain_open_filename("./tmp/check_test.tbchain",O_RDWR | O_CREAT | O_TRUNC);
    fail_if(bc == NULL);
    fs_index_node b = fs_tbchain_new_chain(bc);
    fail_if(fs_tbchain_length(bc, b) != 0);

    fs_rid search[3] = {FS_RID_NULL, FS_RID_NULL, FS_RID_NULL};

    fs_tbchain_it *it = fs_tbchain_new_iterator(bc, FS_RID_NULL, b);
    int count = 0;
    while (fs_tbchain_it_next(it, search) > 0) {
        fail_if(b == 1);
        fail_if(1 == 1);
        count ++;
    }
    fail_if(count !=0 );
    fs_tbchain_it_free(it);

    fs_rid triple[3];
    triple[0] = 0xABC;
    triple[1] = 0xABC;
    triple[2] = 0xABC;
    b = fs_tbchain_add_triple(bc, b, triple);
    fail_if(fs_tbchain_length(bc, b) != 1);

    it = fs_tbchain_new_iterator(bc, FS_RID_NULL, b);
    count = 0;
    while (fs_tbchain_it_next(it, search) > 0) {
        fail_if(b == 1);
        fail_if(search[0] != 0xABC && search[1] != 0xABC && search[2] != 0xABC);
        count ++;
    }
    fail_if(count != 1);
    fs_tbchain_it_free(it);



    fs_index_node c = fs_tbchain_new_chain(bc);
    fs_index_node d = fs_tbchain_new_chain(bc);
    int dcount=0, ccount =0, bcount =1;
    for(i=1; i < 10000; i++) {
        triple[0] = i; triple[1] = i;triple[2] = i + 777;

        b = fs_tbchain_add_triple(bc, b, triple);
        bcount += 1;

        if (c<2) fail_if(1==1);
        if ((i % 7 == 0) && (i % 5 ==0)) {
            c = fs_tbchain_add_triple(bc, c, triple);
            ccount += 1;
        }
        if ((i % 7 == 0) || (i % 5 ==0)) {
            d = fs_tbchain_add_triple(bc, d, triple);
            dcount += 1;
        }
    }

    it = fs_tbchain_new_iterator(bc, FS_RID_NULL, c);
    count = 0;
    while (i>0) {
        i = fs_tbchain_it_next(it, search);
        if (i != 1) {
            fail_if(search[0] != FS_RID_NULL || search[1] != FS_RID_NULL || search[2] != FS_RID_NULL);
            break;
        }
        ccount -= 1;
        int j=0;
        int test = 0;
        for (j=0;j<2 && !test;j++)
            test = (search[j] % 7 == 0) && (search[j] % 5 == 0);
        fail_if(test != 1);
        fail_if(search[2] - 777 != search[1]);
    }
    fail_if(ccount != 0);
    fs_tbchain_it_free(it);

    it = fs_tbchain_new_iterator(bc, FS_RID_NULL, d);
    count = 0;
    i = 1;
    while (i>0) {
        i = fs_tbchain_it_next(it, search);
        if (i != 1) {
            fail_if(search[0] != FS_RID_NULL || search[1] != FS_RID_NULL || search[2] != FS_RID_NULL);
            break;
        }
        dcount -= 1;
        int j=0;
        int test = 0;
        for (j=0;j<2 && !test;j++)
            test = (search[j] % 7 == 0) || (search[j] % 5 == 0);
        fail_if(test != 1);
        fail_if(search[2] - 777 != search[1]);
    }
    fail_if(dcount != 0);
    fs_tbchain_it_free(it);


    it = fs_tbchain_new_iterator(bc, FS_RID_NULL, b);
    count = 0;
    i = 1;
    while (i>0) {
        i = fs_tbchain_it_next(it, search);
        if (i != 1) {
            fail_if(search[0] != FS_RID_NULL || search[1] != FS_RID_NULL || search[2] != FS_RID_NULL);
            break;
        }
        bcount -= 1;
        if (search[0] != 0xABC && bcount > 0)
            fail_if(search[2] - 777 != search[1]);
    }
    fail_if(bcount != 0);
    fs_tbchain_it_free(it);


    fs_tbchain_close(bc);

    TIME(ts_timing_report);
    double end = test_time();
    printf("TIMES: check_tbchain_create %.4f\n", (end - begin));
}
END_TEST

TCase *make_tbchain_tc(void) {
  TCase *tc = tcase_create ("tbchain");
  tcase_set_timeout(tc, 15);
  /* note the tbchain when is backed by a ptree is not tested
   * see FS_TBCHAIN_SUPERSET flag */

  tcase_add_test(tc, check_tbchain_create);
  tcase_add_test(tc, check_tbchain_iterate);
  return tc;
}

