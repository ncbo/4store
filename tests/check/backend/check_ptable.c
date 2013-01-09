#include <check.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include "check_backend.h"

#include "../../../src/backend/ptable.h"

static double test_time()
{
    struct timeval now;

    gettimeofday(&now, 0);

    return (double)now.tv_sec + (now.tv_usec * 0.000001);
}

static int create_ptable(int len_slots,int items, fs_rid *round_pk, fs_row_id *last_row_pk, int *per_count) {
    fs_rid pair[2] = { FS_RID_NULL , FS_RID_NULL };
    int i;
    fs_row_id row;
    for (i = 0; i < len_slots; i++) {
        round_pk[i] = 10000 + i;
        last_row_pk[i] = 0;
        per_count[i] = 0;
    }
    int count_added = 1; /* row 0 is null */
    fs_ptable *pt = fs_ptable_open_filename("./tmp/check_test.ptable",
                                            O_RDWR | O_CREAT | O_TRUNC);
    for (i=0; i < items; i++) {
        int pki = i % len_slots;
        pair[0] = (pki+1) * 7;
        pair[1] = (pki+1) * 3;
        row = fs_ptable_add_pair(pt, last_row_pk[pki], pair);
        if (row == 0) {
            fail_if(1 == 1);
        }
        per_count[pki]++;
        count_added += 1;
        last_row_pk[pki] = row;
    }
    fail_if(count_added != fs_ptable_length(pt));
    fs_ptable_sync(pt);
    fs_ptable_close(pt);
    return count_added;
}

START_TEST (check_ptable_create_values)
{
    fs_row_id row;
    fs_rid pair[2] = { FS_RID_NULL , FS_RID_NULL };
    int LEN_SLOTS = 32;
    fs_rid round_pk[LEN_SLOTS];
    fs_row_id last_row_pk[LEN_SLOTS];
    int per_count[LEN_SLOTS];
    int i;
    int ITS = 5e3 + 777;

    double begin = test_time();
    int count_added = create_ptable(LEN_SLOTS, ITS,round_pk, last_row_pk, per_count);

    double end_add = test_time();
    fs_ptable *pt = fs_ptable_open_filename("./tmp/check_test.ptable",O_RDWR);
    fail_if(count_added != fs_ptable_length(pt));
    for (i=0;i< LEN_SLOTS;i++) {
        row = last_row_pk[i];
        fs_row_id next = row;
        int search_count = 0;
        fail_if(next == 0);
        do {
            int res = fs_ptable_get_row(pt, next, pair);
            if (!res) {
                int p0 = ((int)pair[0]);
                int p1 = ((int)pair[1]);
                fail_if((p0 % 7 != 0) || (p0 % (i+1) != 0));
                fail_if((p1 % 3 != 0) || (p1 % (i+1) != 0));
            }
            fail_if(res == 1, "res 0 in %d %d",i,next);
            search_count++;
        } while((next = fs_ptable_get_next(pt,next)) != 0);
        fail_if(search_count != per_count[i], "Fail count %d %d %d", i, search_count, per_count[i]);
    }
    fs_ptable_close(pt);
    double end = test_time();
    printf("TIMES: check_ptable_create_values %.4f [add %.4f | search %.4f]\n",
            (end - begin),
            (end_add - begin),
            (end - end_add));
}
END_TEST

START_TEST (check_ptable_bench)
{
    fs_row_id row;
    fs_rid pair[2] = { FS_RID_NULL , FS_RID_NULL };
    int LEN_SLOTS = 1024 * 8;
    fs_rid round_pk[LEN_SLOTS];
    fs_row_id last_row_pk[LEN_SLOTS];
    int per_count[LEN_SLOTS];
    int i;
    int ITS = 7e6 + 777;

    double begin = test_time();
    int count_added = create_ptable(LEN_SLOTS, ITS,round_pk, last_row_pk, per_count);

    double end_add = test_time();
    fs_ptable *pt = fs_ptable_open_filename("./tmp/check_test.ptable",O_RDWR);
    fail_if(count_added != fs_ptable_length(pt));
    for (i=0;i< LEN_SLOTS;i++) {
        row = last_row_pk[i];
        fs_row_id next = row;
        int search_count = 0;
        do {
            int res = fs_ptable_get_row(pt, next, pair);
            search_count++;
        } while((next = fs_ptable_get_next(pt,next)) != 0);
        fail_if(search_count != per_count[i], "Fail count %d %d %d", i, search_count, per_count[i]);
    }
    fs_ptable_close(pt);
    double end = test_time();
    printf("TIMES: check_ptable_bench %.4f [add %.4f | search %.4f]\n",
            (end - begin),
            (end_add - begin),
            (end - end_add));
}
END_TEST

START_TEST (check_ptable_add_remove_chain)
{
    fs_ptable *pt = fs_ptable_open_filename("./tmp/check_test.ptable",
                                            O_RDWR | O_CREAT | O_TRUNC);

    int i;
    int a_row = 0;
    int b_row = 0;
    int c_row = 0;
    fs_rid pair[2];
    i = 0;

    double begin = test_time();
    for (i=0; i < 100; i++) {
        pair[0] = i+1;
        pair[1] = i+1;
        a_row = fs_ptable_add_pair(pt, a_row, pair);
    }
    for (i=0; i < 1000; i++) {
        pair[0] = i+1;
        pair[1] = i+1;
        if (i % 2 == 0) {
            b_row = fs_ptable_add_pair(pt, b_row, pair);
            fail_if(b_row == 0);
        } else {
            c_row = fs_ptable_add_pair(pt, c_row, pair);
            fail_if(c_row == 0);
        }
    }

    pair[0] = 0xABC001;
    pair[1] = 0xABC001;
    a_row = fs_ptable_add_pair(pt, a_row, pair);
    pair[0] = 0xABC002;
    pair[1] = 0xABC002;
    a_row = fs_ptable_add_pair(pt, a_row, pair);
    pair[0] = 0xABC003;
    pair[1] = 0xABC003;
    a_row = fs_ptable_add_pair(pt, a_row, pair);

    fs_ptable_remove_chain(pt, a_row);

    pair[0] = 0xCBA003;
    pair[1] = 0xCBA003;
    b_row = fs_ptable_add_pair(pt, b_row, pair);
    fail_if(b_row > 100); /* make sure it reuses rows */
    c_row = fs_ptable_add_pair(pt, c_row, pair);
    fail_if(c_row > 100); /* make sure it reuses rows */

    fs_row_id first;
    fs_row_id row = c_row;
    while ((row = fs_ptable_get_next(pt, row)) != 0) {
        first = row;
    }
    fs_ptable_get_row(pt, first, pair);
    fail_if(pair[0] != 2 || pair[1] != 2 );

    row = b_row;
    while ((row = fs_ptable_get_next(pt, row)) != 0) {
        first = row;
    }
    fs_ptable_get_row(pt, first, pair);
    fail_if(pair[0] != 1 || pair[1] != 1 );

    fs_ptable_remove_chain(pt, c_row);
    fs_ptable_remove_chain(pt, b_row);

    fail_if(fs_ptable_free_length(pt) != fs_ptable_length(pt));
    fs_ptable_close(pt);

    double end = test_time();

    printf("TIMES: check_ptable_add_remove_chain %.4f\n", (end - begin));
}
END_TEST

START_TEST (check_ptable_add_remove_pair)
{
    fs_ptable *pt = fs_ptable_open_filename("./tmp/check_test.ptable",
                                            O_RDWR | O_CREAT | O_TRUNC);
    double begin = test_time();
    int i;
    int a_row = 0;
    int b_row = 0;
    int c_row = 0;
    fs_rid pair[2];
    i = 0;

    for (i=0; i < 100; i++) {
        pair[0] = (i+1) % 7;
        if (i == 33) pair[0] = 0xABC00001;
        pair[1] = i+1;
        a_row = fs_ptable_add_pair(pt, a_row, pair);
    }
    for (i=0; i < 1000; i++) {
        pair[0] = (i+1) % 17;
        if (i == 33) pair[0] = 0xABC00001;
        pair[1] = i+1;
        if (i % 2 == 0) {
            b_row = fs_ptable_add_pair(pt, b_row, pair);
            fail_if(b_row == 0);
        } else {
            c_row = fs_ptable_add_pair(pt, c_row, pair);
            fail_if(c_row == 0);
        }
    }
    int removed = 0;
    int all_removed = 0;
    pair[0] = 0xABC00001;
    pair[1] = FS_RID_NULL;
    fs_row_id pre = a_row;
    a_row = fs_ptable_remove_pair(pt, a_row, pair, &removed, NULL);
    fail_if(pre != a_row);
    fail_if(removed != 1);
    for (i = a_row; i; i=fs_ptable_get_next(pt, i)) {
        fs_ptable_get_row(pt, i, pair);
        if (pair[0] == 0xABC00001)
            break;
    }
    fail_if(i > 0);
    for (i = c_row; i; i=fs_ptable_get_next(pt, i)) {
        fs_ptable_get_row(pt, i, pair);
        if (pair[0] == 0xABC00001)
            break;
    }
    fail_if(i == 0);
    all_removed += removed;

    pair[0] = FS_RID_NULL;
    pair[1] = 50;
    pre = a_row;
    fs_rid_set *models = fs_rid_set_new();
    removed = 0;
    a_row = fs_ptable_remove_pair(pt, a_row, pair, &removed, models);
    fail_if(pre != a_row); /*not deleting last */
    fail_if(removed == 0);
    fail_if(!fs_rid_set_contains(models, 1)); /* 1 == 50 % 7 */
    for (i = a_row; i; i=fs_ptable_get_next(pt, i)) {
        fs_ptable_get_row(pt, i, pair);
        if (pair[1] == 50) fail_if(1 == 1);
    }
    fs_rid_set_free(models);
    all_removed += removed;


    pair[0] = 16;
    pair[1] = 50;
    pre = a_row;
    removed = 0;
    c_row = fs_ptable_remove_pair(pt, c_row, pair, &removed, NULL);
    fail_if(pre != a_row); /*not deleting last */
    fail_if(removed == 0);
    for (i = c_row; i; i=fs_ptable_get_next(pt, i)) {
        fs_ptable_get_row(pt, i, pair);
        if (pair[1] == 50 && pair[0] == 16) fail_if(1 == 1);
    }
    all_removed += removed;
    fail_if(fs_ptable_free_length(pt) != (all_removed+1));

    pair[0] = FS_RID_NULL;
    pair[1] = FS_RID_NULL;
    removed = 0;
    models = fs_rid_set_new();
    removed = 0;
    b_row = fs_ptable_remove_pair(pt, b_row, pair, &removed, models);
    fail_if(removed == 0);
    for (i = 0; i < 17; i++)
        fail_if(!fs_rid_set_contains(models, i));
    all_removed += removed;
    fail_if(fs_ptable_free_length(pt) != (all_removed+1));
    fs_rid_set_free(models);

    pair[0] = 0xCCCC;
    pair[1] = 0xABC222222;
    c_row = fs_ptable_add_pair(pt, c_row, pair);
    pair[0] = 0xCCCD;
    pair[1] = 0xABC222222;
    c_row = fs_ptable_add_pair(pt, c_row, pair);
    pair[0] = 0xCCCE;
    pair[1] = 0xABC222222;
    c_row = fs_ptable_add_pair(pt, c_row, pair);
    pair[0] = FS_RID_NULL;
    pair[1] = 0xABC222222;
    removed = 0;
    models = fs_rid_set_new();
    c_row = fs_ptable_remove_pair(pt, c_row, pair, &removed, models);
    fail_if(removed != 3);
    fail_if(!fs_rid_set_contains(models, 0xCCCC));
    fail_if(!fs_rid_set_contains(models, 0xCCCD));
    fail_if(!fs_rid_set_contains(models, 0xCCCE));
    fs_rid_set_free(models);

    pair[1] = FS_RID_NULL;

    fs_ptable_close(pt);
}
END_TEST

TCase *make_ptable_tc(void) {
  TCase *tc = tcase_create ("ptree");
  tcase_add_test (tc, check_ptable_create_values);
  tcase_add_test (tc, check_ptable_bench);
  tcase_add_test (tc, check_ptable_add_remove_chain);
  tcase_add_test (tc, check_ptable_add_remove_pair);
  tcase_set_timeout(tc, 15);
  return tc;
}
