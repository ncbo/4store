#ifndef QUERY_REWRITE_H
#define QUERY_REWRITE_H

#include <rasqal.h>
#include <raptor.h>
#include "query-datatypes.h"
#include "../common/4store.h"

#define QUERY_REWRITE_MAX_RULES 32
#define QUERY_REWRITE_MAX_BIND_PER_BLOCK 256
#define QUERY_REWRITE_MAX_ANT 8
#define QUERY_REWRITE_MAX_CONS 4
#define QUERY_REWRITE_MAX_EXPANSION 256

#define QUERY_REWRITE_MAX_FANOUT 256
#define QUERY_REWRITE_MAX_BLOCKS 256

#define QUERY_REWRITE_INPUT_QUERY 0
#define QUERY_REWRITE_INPUT_BIND 1


#define RULE_VAR 0
#define RULE_CONSTANT 1

/**
* A rule is a the combination of:
*   list of antecedents as a list of rasqal_triples.
*   list of ancetecents is treated as conjuctive query to match triples.
*   list of consequents as a list of rasqal_triples
*
* A four digit char array identifies the rule.
* For simiplicity we deal with limited arrays for
* ancetecents and consequences.
**/

typedef rasqal_literal fs_rule_literal;

typedef struct expansion_input {
    fs_rid_vector **query_bind;
    rasqal_variable **vars;
    rasqal_variable **initial_vars;
    int bind_replace;
    int tobind;
    int call;
} fs_rule_input;

typedef struct __rule_triple {
    rasqal_triple *rt;
    /* being a vector leaves potential to implement OR operations */
    fs_rid_vector **vrid_quad; 
    rasqal_variable **vars;
    int rule_block;

    /* reference to the rule input that 
       created this condition */
    fs_rule_input *rule_input;

} fs_rule_bind;

typedef struct __rule_triple_list {
    fs_rule_bind *binds[QUERY_REWRITE_MAX_BIND_PER_BLOCK+1];
    int is_copy[QUERY_REWRITE_MAX_BIND_PER_BLOCK+1];
    fs_binding *result_binding;
    int expanded;
    int iteration;
    int rule_id;
} fs_rule_bind_block;

typedef struct _fs_rule_query {
    fs_rule_input *rule_input;
    fs_query *query;
    int block;
    int count;
    fs_rule_bind_block *blocks[QUERY_REWRITE_MAX_BLOCKS+1];
} fs_rule_query;


typedef struct _fs_rule {
    fs_rule_bind antecedents[QUERY_REWRITE_MAX_ANT+1];
    fs_rule_bind consequents[QUERY_REWRITE_MAX_CONS+1];
    fs_rule_bind exceptions[QUERY_REWRITE_MAX_CONS+1];
    unsigned char *id;
    int exec_flag;
    int iterate_on_empty;
    unsigned int maxrec;
} fs_rule;

/**
* fs_rule_context maintains an instance of rules in
* the system.
* Version 0.1 for simplicity, theres a constant limit of 
**/
typedef struct _fs_rule_context {
    fs_rule rules[QUERY_REWRITE_MAX_RULES+1];

    /* internal */
    rasqal_world *rasqalw;
    raptor_world *raptorw;

} fs_rule_context;



fs_rule_query * fs_rule_create_query(fs_query *, int , fs_rule_input *);
int fs_rule_query_apply(fs_rule_query *);


rasqal_variable ** fs_rule_vars_in_quad_position(rasqal_variable **vars,int tobind);
fs_rid_vector **fs_rule_copy_slots(fs_rid_vector **slots);
void fs_rule_query_free(fs_rule_query *);
void fs_rule_input_free(fs_rule_input *);
int fs_rule_flags_from_string(const char *srules);
void fs_rule_extend_results(fs_rule_query *rule_query,rasqal_variable **vars, fs_rid_vector ***results);
#endif
