#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>

#include "query-rewrite.h"
#include "query-intl.h"
#include "query.h"
#include "results.h"
#include "../common/4s-hash.h"
#include "../common/error.h"

//#define DEBUG_RULES 0

static fs_rule_context gbl_rule_context;
static GStaticMutex rule_init_mutex;

static const unsigned char *__new_cpy_str(const char *in_str);

# define __CONTEXT_IS_INIT() \
    (gbl_rule_context.rasqalw != NULL)

#define __RQ_W() \
    (gbl_rule_context.rasqalw)

#define __RP_W() \
    (gbl_rule_context.raptorw)

#define __NEW_LITERAL_URI(uri) \
    (rasqal_new_uri_literal(__RQ_W(),raptor_new_uri(__RP_W(),uri)))

#define __NEW_LITERAL_VAR(l,n) do {\
    l->type = RASQAL_LITERAL_VARIABLE;\
    l->value.variable = calloc(1, sizeof(rasqal_variable));\
    l->value.variable->name = n;\
    } while(0)

#define __SET_ID(r,p) \
    gbl_rule_context.rules[r].id = (unsigned char *) p;

#define __SET_EXEC_FLAG(r,f) \
    gbl_rule_context.rules[r].exec_flag = f;

#define __SET_MAXREC(r,i) \
    gbl_rule_context.rules[r].maxrec = i;

#define __SET_ITERATEONEMPTY(r,i) \
    gbl_rule_context.rules[r].iterate_on_empty = i;

#define __GET_RULE(i) (&(gbl_rule_context.rules[i]))
    
#define FOR_EACH_RULE(i,r) \
    for (r=&(gbl_rule_context.rules[i]); r->id; r=&(gbl_rule_context.rules[++i]))

#define FOR_EACH_CONDITION(i,rule,t,c) \
    for (c=&(rule->t[i]); c->rt; c=&(rule->t[++i]))

#define SET_TRIPLE_IN_RULE(i,j,t,s,p,o) do{\
         gbl_rule_context.rules[i].t[j].rt = rasqal_new_triple((rasqal_literal *)s,(rasqal_literal *)p,(rasqal_literal *)o);\
         } while (0)

#define RQ_LIT_TYPE(t) \
         (((rasqal_literal *) t)->type)

#define RQ_LIT_IS_VAR(t) \
         (RQ_LIT_TYPE((rasqal_literal *) t) == RASQAL_LITERAL_VARIABLE)

#define RQ_LIT_IS_URI(t) \
         (RQ_LIT_TYPE((rasqal_literal *) t) == RASQAL_LITERAL_URI)

#define RQ_VAR_NAME_FROM_LIT(l) \
        (((rasqal_literal *) l)->value.variable->name)

#define RQ_VAR_NAME(l) \
        ((const char *) (l)->name)

#define CMP_URIS(a,b,err) \
        (rasqal_literal_compare((rasqal_literal *) a,(rasqal_literal *) b,RASQAL_COMPARE_URI,&err))

#define CMP_LIT(a,b,err) \
        (rasqal_literal_compare((rasqal_literal *) a,(rasqal_literal *) b,0,&err))

#define CMP_LIT_VAR(a,b) \
        (strcmp((const char * )a->value.variable->name,(const char *)b->value.variable->name))

#define CMP_VAR(a,b) \
        (strcmp((const char * )a->name,(const char *)b->name))

#define TRIPLE_TO_ARRAY(t) { (fs_rule_literal *) (t)->subject, (fs_rule_literal *) (t)->predicate, (fs_rule_literal *) (t)->object }

#define TRIPLE_TO_ARRAY_WITH_ORIGIN(t) { (fs_rule_literal *) (t)->origin, (fs_rule_literal *) (t)->subject, \
                                         (fs_rule_literal *) (t)->predicate, (fs_rule_literal *) (t)->object }\

#define SET_LITERAL_COMPONENT(cond, i, lit) do {\
        switch(i) {\
            case 0: cond->rt->subject = lit; break; \
            case 1: cond->rt->predicate = lit; break; \
            case 2: cond->rt->object = lit; break; \
            default: break; } } while(0)

#define ASSERT_EXPANSION_IN_BOUNDS(p) \
       if (p >= QUERY_REWRITE_MAX_EXPANSION) {\
            fs_error(LOG_ERR,"Rule rewriting expansion hit MAX");\
            return -1; }

#define SEEK_NEXT_SLOT_FOR_EXPANSION(arr,p) do {\
        while (arr[p] != NULL && p < QUERY_REWRITE_MAX_EXPANSION) position++;\
            ASSERT_EXPANSION_IN_BOUNDS(p)\
        } while(0)

#define APPEND_CONDITION(arr,p,c) do{\
        arr[p++]=c;\
        ASSERT_EXPANSION_IN_BOUNDS(p)\
        } while(0)

struct rid_or_var {
    fs_rid_vector *vrid;
    rasqal_variable *var;
};

struct translation_table {
    struct rid_or_var table[4][2];
};

void fs_rule_free_rasqal_literal(rasqal_literal *lit) {
    if (lit == NULL)
        return;
    
    if (lit->type == RASQAL_LITERAL_VARIABLE) {
        free(lit);
    }
}

void fs_rule_free_rasqal_triple(rasqal_triple *rt) {
    if (rt == NULL)
        return;
    
    fs_rule_free_rasqal_literal(rt->origin);
    fs_rule_free_rasqal_literal(rt->subject);
    fs_rule_free_rasqal_literal(rt->predicate);
    fs_rule_free_rasqal_literal(rt->object);

    free(rt);
}



void fs_rule_free_rule_bind(fs_rule_bind *rb, int incl) {
    if (rb == NULL)
        return;

    fs_rule_free_rasqal_triple(rb->rt);

    if (rb->vrid_quad != NULL) {
        for (int i=0; i < 4; i++) {
            if (rb->vrid_quad[i])
                fs_rid_vector_free(rb->vrid_quad[i]);
        }
        free(rb->vrid_quad);
    }
    if (rb->vars) {
       for (int i=0; i < 4; i++) {
           if (rb->vars[i]) {
              free((void *)rb->vars[i]->name);
              free(rb->vars[i]);
           }
       }
       free(rb->vars); 
    }
    if (incl) {
        free(rb);
    }
}

void fs_rule_free_bind_block(fs_rule_bind_block *block) {
    if (block == NULL)
        return;

    int bi = 0;
    while(bi < QUERY_REWRITE_MAX_BIND_PER_BLOCK &&
        block->binds[bi] != NULL) {
        if (!block->is_copy[bi])
            fs_rule_free_rule_bind(block->binds[bi], 1);
        bi++;
    }

    if (block->result_binding) {
        fs_binding_free(block->result_binding);
    }

    free(block);
}


void fs_free_rule(fs_rule *rule) {
    if (rule == NULL)
        return;

    fs_rule_bind *cond;
    int j=0;
    FOR_EACH_CONDITION(j,rule,antecedents,cond) {
        fs_rule_free_rule_bind(cond, 0);
    }
    j=0;
    FOR_EACH_CONDITION(j,rule,consequents,cond) {
        fs_rule_free_rule_bind(cond, 0);
    }
    j=0;
    FOR_EACH_CONDITION(j,rule,exceptions,cond) {
        fs_rule_free_rule_bind(cond, 0);
    } 
    free(rule);
}

void fs_rule_query_free(fs_rule_query *rule_query) {
    if (!rule_query)
        return;

    int b = 0;
    while(b < QUERY_REWRITE_MAX_BLOCKS &&
        rule_query->blocks[b] != NULL) {
        fs_rule_free_bind_block(rule_query->blocks[b]);
        b++;
    }
    free(rule_query);
}


void fs_rule_input_free(fs_rule_input *rule_input) {
    if (!rule_input)
        return;
        
    if (rule_input->query_bind) {
        for (int i=0; i < 4; i++) {
            if (rule_input->query_bind[i])
                fs_rid_vector_free(rule_input->query_bind[i]);
        }
        free(rule_input->query_bind);
    }

    if (rule_input->vars)
        free(rule_input->vars);
        
    free(rule_input);
}

rasqal_variable *fs_rule_rasqal_var_copy(rasqal_variable *v) {
    if (!v)
        return NULL;

    rasqal_variable *c = calloc(1, sizeof(rasqal_variable)); 
    c->name = (unsigned char *) strdup((const char *)v->name);
    c->user_data = NULL;
    return c;
}

rasqal_variable **fs_rule_copy_vars(rasqal_variable **vars) {
    rasqal_variable **copy  = (rasqal_variable **) calloc(4 , sizeof(rasqal_variable *));
    for (int i=0; i < 4; i++) {
       if (vars[i])
           copy[i] = fs_rule_rasqal_var_copy(vars[i]); 
    }
    return copy;
}

fs_rid_vector **fs_rule_copy_slots(fs_rid_vector **slots) {
    fs_rid_vector **copy = (fs_rid_vector **) calloc(4 , sizeof(fs_rid_vector *));
    for (int i=0; i < 4; i++) {
        if (slots[i]) {
            copy[i] = fs_rid_vector_new(0);
            fs_rid_vector_append_vector(copy[i],slots[i]);
        }
    }
    return copy;
}



/* at least one query element has some RID values */
int __fs_rule_table_bind_ground(struct translation_table *ttable) {
    for (int i=0; i<4; i++) {
        if (ttable->table[i][1].vrid != NULL) {
            if (ttable->table[i][1].vrid->length == 0) {
                return 0;
            }
        }
    }
    return 1;
}

const unsigned char *__new_cpy_str(const char *in_str) {
    const unsigned char *str_cpy = 
        (unsigned char *) malloc((strlen(in_str)+1) * sizeof(unsigned char));
    strcpy((char *) str_cpy,(char *) in_str);
    return str_cpy;
}

void __set_worlds(fs_query *q) {
    __RQ_W() = q->qs->rasqal_world;
    __RP_W() = q->qs->raptor_world;
}

fs_rule_literal *__fs_rule_new_constant(const char *l) {
    return __NEW_LITERAL_URI(__new_cpy_str(l));
}

fs_rule_literal *__fs_rule_new_variable(const char *name) {
    rasqal_literal *lit = rasqal_new_uri_literal(__RQ_W(),NULL);
    __NEW_LITERAL_VAR(lit,__new_cpy_str(name));
    return lit;
}

void __init_bind(fs_rule_bind *cond) {
    if (cond->vrid_quad == NULL) {
        cond->vrid_quad = calloc(4,sizeof(fs_rid_vector *));
    }
    if (cond->vars == NULL) {
        cond->vars = calloc(4, sizeof(rasqal_variable *));
    }
}

void __set_rid_quad(fs_rule_bind *cond) {
    __init_bind(cond);

    int i=0;
    fs_rule_literal *quad[4] = TRIPLE_TO_ARRAY_WITH_ORIGIN(cond->rt); 
    for (i=0;i<4;i++) {
        if (quad[i] && RQ_LIT_IS_URI(quad[i])) {
            fs_rid rid_val = 
                fs_hash_uri((const char *)raptor_uri_as_string(quad[i]->value.uri));
            cond->vrid_quad[i] = fs_rid_vector_new(0); 
            fs_rid_vector_append(cond->vrid_quad[i], rid_val);
            cond->vars[i] = NULL;
        }
        else if (quad[i] && RQ_LIT_IS_VAR(quad[i])) {
            cond->vars[i] = quad[i]->value.variable;
        }
        else {
            /* not set */
            cond->vrid_quad[i] = NULL;
            cond->vars[i] = NULL;
        }
    }
}

void __create_quads_rids(int rule_i) {
    fs_rule *rule = __GET_RULE(rule_i);
    int j=0;
    fs_rule_bind *cond; 
    FOR_EACH_CONDITION(j,rule,antecedents,cond) {
        __set_rid_quad(cond);
    }
    j=0;
    FOR_EACH_CONDITION(j,rule,consequents,cond) {
        __set_rid_quad(cond);
    }
    j=0;
    FOR_EACH_CONDITION(j,rule,exceptions,cond) {
        __set_rid_quad(cond);
    }
}

void __create_rule_subp(int rule_i) {
    fs_rule_literal *x = __fs_rule_new_variable("__x1");
    fs_rule_literal *y = __fs_rule_new_variable("__y1");
    fs_rule_literal *p = __fs_rule_new_variable("__p1");
    fs_rule_literal *pp = __fs_rule_new_variable("__pp1");

    fs_rule_literal *none = __fs_rule_new_variable("none");

    fs_rule_literal *sp = __fs_rule_new_constant("http://www.w3.org/2000/01/rdf-schema#subPropertyOf");
    fs_rule_literal *sc = __fs_rule_new_constant("http://www.w3.org/2000/01/rdf-schema#subClassOf");
    fs_rule_literal *eq = __fs_rule_new_constant("http://www.w3.org/2002/07/owl#equivalentClass");

    SET_TRIPLE_IN_RULE(rule_i,0,antecedents,x,pp,y);
    SET_TRIPLE_IN_RULE(rule_i,1,antecedents,pp,sp,p);
    SET_TRIPLE_IN_RULE(rule_i,0,exceptions,none,sp,none);
    SET_TRIPLE_IN_RULE(rule_i,1,exceptions,none,sc,none);
    SET_TRIPLE_IN_RULE(rule_i,2,exceptions,none,eq,none);
    SET_TRIPLE_IN_RULE(rule_i,0,consequents,x,p,y);
    __SET_ID(rule_i,__new_cpy_str("SUBP"));
    __SET_EXEC_FLAG(rule_i,0x0001);
    __SET_MAXREC(rule_i,5);
    __SET_ITERATEONEMPTY(rule_i,1);
    __create_quads_rids(rule_i);
}

void __create_rule_subc(int rule_i) {
    fs_rule_literal *a = __fs_rule_new_variable("__classA");
    fs_rule_literal *b = __fs_rule_new_variable("__classB");
    fs_rule_literal *c = __fs_rule_new_variable("__classC");

    fs_rule_literal *sc = 
        __fs_rule_new_constant("http://www.w3.org/2000/01/rdf-schema#subClassOf");

    SET_TRIPLE_IN_RULE(rule_i,0,antecedents,a,sc,b);
    SET_TRIPLE_IN_RULE(rule_i,1,antecedents,b,sc,c);
    SET_TRIPLE_IN_RULE(rule_i,0,consequents,a,sc,c);
    __SET_ID(rule_i,__new_cpy_str("SUBC"));
    __SET_EXEC_FLAG(rule_i,0x0002);
    __SET_MAXREC(rule_i,35);
    __create_quads_rids(rule_i);
}

void __create_rule_subp_transitive(int rule_i) {
    fs_rule_literal *a = __fs_rule_new_variable("__propA");
    fs_rule_literal *b = __fs_rule_new_variable("__propB");
    fs_rule_literal *c = __fs_rule_new_variable("__propC");

    fs_rule_literal *sp = 
        __fs_rule_new_constant("http://www.w3.org/2000/01/rdf-schema#subPropertyOf");

    SET_TRIPLE_IN_RULE(rule_i,0,antecedents,a,sp,b);
    SET_TRIPLE_IN_RULE(rule_i,1,antecedents,b,sp,c);
    SET_TRIPLE_IN_RULE(rule_i,0,consequents,a,sp,c);
    __SET_ID(rule_i,__new_cpy_str("SUBPT"));
    __SET_EXEC_FLAG(rule_i,0x0004);
    __SET_MAXREC(rule_i,5);
    __SET_ITERATEONEMPTY(rule_i,0);
    __create_quads_rids(rule_i);
}

void __create_rule_inverse(int rule_i) {
    fs_rule_literal *x = __fs_rule_new_variable("__x1");
    fs_rule_literal *y = __fs_rule_new_variable("__y1");
    fs_rule_literal *p = __fs_rule_new_variable("__p1");
    fs_rule_literal *pp = __fs_rule_new_variable("__pp1");

    fs_rule_literal *inverse = __fs_rule_new_constant("http://www.w3.org/2002/07/owl#inverseOf");

    SET_TRIPLE_IN_RULE(rule_i,0,antecedents,x,p,y);
    SET_TRIPLE_IN_RULE(rule_i,1,antecedents,pp,inverse,p);
    SET_TRIPLE_IN_RULE(rule_i,0,consequents,y,pp,x);
    __SET_ID(rule_i,__new_cpy_str("INV"));
    __SET_EXEC_FLAG(rule_i,0x0008);
    __SET_MAXREC(rule_i,1);
    __SET_ITERATEONEMPTY(rule_i,0);
    __create_quads_rids(rule_i);
}

void __fs_rule_context_init(fs_query *q) {
   __set_worlds(q); 
   int rule_i=0;
   __create_rule_subp(rule_i++);
   //__create_rule_inverse(rule_i++);
   __create_rule_subc(rule_i++);
   //__create_rule_subp_transitive(rule_i++);
}

void fs_rule_context_init_sync(fs_query *q) {
    if (__CONTEXT_IS_INIT())
        return;

    g_static_mutex_lock(&rule_init_mutex);
    if (!__CONTEXT_IS_INIT()) {
        __fs_rule_context_init(q);    
    }
    g_static_mutex_unlock(&rule_init_mutex);
}

static inline int fs_rule_var_is_internal(rasqal_variable *var) {
    return var->name[0] == '@';
}

fs_rule_bind *__fs_rule_rid_copy(fs_rule_bind *cond) {
    fs_rule_bind *cpy = (fs_rule_bind *) calloc(1, sizeof(fs_rule_bind));
    cpy->rt = NULL;
    if (cond->vars)
        cpy->vars = fs_rule_copy_vars(cond->vars);
    if (cond->vrid_quad)
        cpy->vrid_quad = fs_rule_copy_slots(cond->vrid_quad);
    return cpy;
}

void fs_rule_bind_print(fs_rule_bind *cond) {
    for (int i=0; i < 4; i++) {
        if (cond->vars[i])
            fprintf(stdout," %d = %s ", i, cond->vars[i]->name);
        if (cond->vrid_quad[i]) {
            fprintf(stdout,"[%d]=(",i);
            for (int j=0; j < cond->vrid_quad[i]->length && j < 6; j++) {
                fprintf(stdout," %llx ", cond->vrid_quad[i]->data[j]);
            }
            fprintf(stdout,"(%d)", cond->vrid_quad[i]->length);
            fprintf(stdout,") ");
        } else {
            fprintf(stdout,"[%d] = not set",i);
        }
    }
    fprintf(stdout,"\n");
}

void fs_rule_bind_block_print(fs_rule_bind_block *block) {
    fprintf(stdout,"fs_rule_bind_block_print %p INIT\n",block);
    int b=0;
    while(b < QUERY_REWRITE_MAX_BIND_PER_BLOCK &&
            block->binds[b] != NULL) {
            fs_rule_bind_print(block->binds[b]);
            b++;
    }
    fprintf(stdout,"fs_rule_bind_block_print %p END\n",block);
}

fs_rule_literal *fs_copy_literal_with_seed(fs_rule_literal *lit, int seed) {
    if (!lit)
        return NULL;

    if (lit->type != RASQAL_LITERAL_VARIABLE)
        return lit;
    gchar *s = g_strdup_printf("@%s_s%d",lit->value.variable->name,seed);
    fs_rule_literal *r = __fs_rule_new_variable(s); 
    g_free(s);
    return r;
}

 rasqal_triple *fs_copy_triple_with_seed(rasqal_triple *rt,int seed) {
    rasqal_triple *copy = calloc(1,sizeof(rasqal_triple));
    copy->origin = fs_copy_literal_with_seed(rt->origin,seed);
    copy->subject = fs_copy_literal_with_seed(rt->subject,seed);
    copy->predicate = fs_copy_literal_with_seed(rt->predicate,seed);
    copy->object = fs_copy_literal_with_seed(rt->object,seed);
    return copy;
}

 fs_rule* fs_rule_copy(fs_rule *rule,int seed) {
    fs_rule *copy = calloc(1, sizeof(fs_rule));
    fs_rule_bind *cond;
    int j=0;
    FOR_EACH_CONDITION(j,rule,antecedents,cond) {
        copy->antecedents[j].rt = fs_copy_triple_with_seed(cond->rt,seed);
        copy->antecedents[j].rule_block = rule->antecedents[j].rule_block;
        __set_rid_quad(&copy->antecedents[j]);
    }
    j=0;
    FOR_EACH_CONDITION(j,rule,consequents,cond) {
        copy->consequents[j].rt = fs_copy_triple_with_seed(cond->rt,seed);
        copy->consequents[j].rule_block = rule->consequents[j].rule_block;
        __set_rid_quad(&copy->consequents[j]);
    }
    j=0;
    FOR_EACH_CONDITION(j,rule,exceptions,cond) {
        copy->exceptions[j].rt = fs_copy_triple_with_seed(cond->rt,seed);
        copy->exceptions[j].rule_block = rule->consequents[j].rule_block;
        __set_rid_quad(&copy->exceptions[j]);
    }
    copy->maxrec = rule->maxrec;
    return copy;
}

void __fs_rule_rid_or_var_print(struct rid_or_var *r) {
    if (r->var != NULL)
        printf(" (var) %s ",r->var->name);
    if (r->vrid != NULL) {
        printf(" (rids %d) ", r->vrid->length); 
        for (int i=0; i < r->vrid->length && i < 6; i++) { 
            printf(" %llx ",r->vrid->data[i]);
        }
    } 
    if (!r->var && !r->vrid)
         printf(" null ");
}

void ____fs_rule_translation_table_print(struct translation_table *ttable) {
    for (int i=0; i<4; i++) {
        struct rid_or_var *row = ttable->table[i];
        struct rid_or_var *rule_conseq = &row[0];
        struct rid_or_var *rule_bind = &row[1];
        printf("  %d ",i);
        __fs_rule_rid_or_var_print(rule_conseq);
        printf("  | ");
        __fs_rule_rid_or_var_print(rule_bind);
        printf("\n");
    }
}

int __fs_rule_translation_table_fill(
                            struct translation_table *ttable,
                            fs_rule *rule,
                            fs_rule_bind *b) {
    
    fs_rid_vector **bind = b->vrid_quad;
    rasqal_variable **vars = b->vars;
    rasqal_variable **rule_vars =  rule->consequents[0].vars;
    fs_rid_vector **vrid_rule = rule->consequents[0].vrid_quad;
    for (int i=0; i<4; i++) {
        /* rule part is NOT var */
        if (!rule_vars[i]) { 
            /* bind part is NOT var */ 
            if (bind[i] && bind[i]->length == 1  && bind[i]->data[0] != FS_RID_NULL) { 
            if (vrid_rule[i] && vrid_rule[i]->length == 1  
                    && vrid_rule[i]->data[0] != FS_RID_NULL) { 
                 if (vrid_rule[i]->data[0] != bind[i]->data[0]) {
                   return -1;
                 }
            }}
        }
        struct rid_or_var *row = ttable->table[i];
        struct rid_or_var *rule_conseq = &row[0];
        struct rid_or_var *rule_bind = &row[1];
        /* rule conseq */
        if (rule->consequents[0].vars[i])
            rule_conseq->var = rule->consequents[0].vars[i];
        else 
            rule_conseq->var = NULL;
        
        if (vrid_rule[i] && vrid_rule[i]->length > 0) {
            rule_conseq->vrid = vrid_rule[i];
        } else 
            rule_conseq->vrid = NULL;

        /* bind */
        if (fs_rid_vector_length(bind[i]) > 0) {
            rule_bind->vrid = bind[i];
        } else
            rule_bind->vrid = NULL; 
            
        if (vars[i])
            rule_bind->var = vars[i];
        else
            rule_bind->var = NULL;
    }
#if DEBUG_RULES > 0
    ____fs_rule_translation_table_print(ttable);
#endif
    return 1;
}

rasqal_variable ** fs_rule_vars_in_quad_position(rasqal_variable **vars,int tobind) {
    int bind_flags [4] = { FS_BIND_MODEL, FS_BIND_SUBJECT, FS_BIND_PREDICATE, FS_BIND_OBJECT};
    rasqal_variable **res = calloc(4, sizeof(rasqal_variable *));

    int offset=0;
    for (int x = 0; x < 4; x++) {
        if(tobind & bind_flags[x]) {
            if (vars[offset]) {
                res[x]=vars[offset];
            }
            offset++;
        }
    }
    return res;
}

int __fs_rule_bind_contains_query_vars(fs_rule_bind *bind) {
    for (int i=0;i<4;i++) {
        if (bind->vars[i] &&
            !fs_rule_var_is_internal(bind->vars[i]))
            return 1;
    }
    return 0;
}

int fs_rule_bind_expand(fs_rule_query *rq, fs_rule_bind_block *block,
                                int bind_index, fs_rule *rule) {

    fs_rule_bind *bind = block->binds[bind_index];
    int j=0;
    fs_rule_bind *exc;
    FOR_EACH_CONDITION(j,rule,exceptions,exc) {
       for (int t=0; t<4; t++) {
            if (!exc->vars[t] && !bind->vars[t]) {
                if (!(bind->vrid_quad[t] && exc->vrid_quad[t]))
                    continue;
                if (bind->vrid_quad[t]->data[0] == exc->vrid_quad[t]->data[0]) {
                    return 0;
                }
            }
       }
    }
    struct translation_table ttable;

    if (__fs_rule_translation_table_fill(&ttable,rule,bind) < 0)
        return 0;

    int ground = __fs_rule_table_bind_ground(&ttable);
    if (!ground)
        return 0;

    j=0;
    int res=0;
    fs_rule_bind *antc;
    /* if comp(i,bind) == comp(i,rule_consq) then replace(comp(i,ant),comp(i,bind)) */
    FOR_EACH_CONDITION(j,rule,antecedents,antc) {
        fs_rule_bind *ant_copy = __fs_rule_rid_copy(antc);

        /* model is treated differently */
        if (ttable.table[0][1].vrid != NULL &&
            ttable.table[0][1].vrid->length > 0) {
            if (ant_copy->vrid_quad[0])
                fs_rid_vector_free(ant_copy->vrid_quad[0]);
            ant_copy->vrid_quad[0] = fs_rid_vector_new(0);
            fs_rid_vector_append_vector(ant_copy->vrid_quad[0],
                                        ttable.table[0][1].vrid);                
       }

        for (int i=1; i<4; i++) { /* for each m,s,p,o in table */
            for (int t=1; t<4; t++) { /* for each m,s,p,o in antc */
                if (ttable.table[i][0].var != NULL &&
                    antc->vars[t] != NULL &&
                    !CMP_VAR(ttable.table[i][0].var, antc->vars[t])) {

                    if (ant_copy->vrid_quad[t])
                        fs_rid_vector_free(ant_copy->vrid_quad[t]);
                    if (ant_copy->vars[t]) {
                        free((void *)ant_copy->vars[t]->name);
                        free(ant_copy->vars[t]);
                    }

                    if (ttable.table[i][1].vrid != NULL) {
                        ant_copy->vrid_quad[t] = fs_rid_vector_new(0);
                        fs_rid_vector_append_vector(ant_copy->vrid_quad[t],
                                                    ttable.table[i][1].vrid);
                    } else
                        ant_copy->vrid_quad[t] = NULL;

                    if (ttable.table[i][1].var != NULL){
                        ant_copy->vars[t] = fs_rule_rasqal_var_copy(ttable.table[i][1].var);
                    } else 
                        ant_copy->vars[t] = NULL;

                } 
            }
        }
        block->binds[bind_index + res] = ant_copy;
        block->is_copy[bind_index + res] = 0;
        res++;
    }
    return res;
}

fs_rule_bind_block *fs_rule_bind_block_expand_with_rule(fs_rule_query *rq, 
                                            fs_rule_bind_block *block, fs_rule *rule) {
    fs_rule_bind_block *nb = calloc(1, sizeof(fs_rule_bind_block));
    nb->expanded = 1;

    int bi=0;
    int bnew=0;
    while(bi < QUERY_REWRITE_MAX_BIND_PER_BLOCK && block->binds[bi] != NULL) {
        fs_rule_bind *bind = block->binds[bi];
        nb->binds[bnew] = block->binds[bi];
        nb->is_copy[bnew] = 1;
        if (__fs_rule_bind_contains_query_vars(bind)) {
            int new_binds = fs_rule_bind_expand(rq, nb, bnew, rule);
            bnew += new_binds; /* skip expansion of the newly expanded binds */
        } else {
            bnew++;
        }
        bi++;
    }
    if (!bnew) {
        fs_rule_free_bind_block(nb);
        return NULL;
    }
    return nb;
}

int fs_rule_expand_bind_block_expand(fs_rule_query *rq, 
                                     fs_rule_bind_block *block, int calln) {
    int b=0;
    while(b<QUERY_REWRITE_MAX_BLOCKS && rq->blocks[b] != NULL)
        b++;
    if (b == QUERY_REWRITE_MAX_BLOCKS) {
        fs_error(LOG_ERR,"QUERY_REWRITE_MAX_BLOCKS reached");
        return 0;
    }

    int i=0;
    int blocks_created = 0;
    fs_rule *rule=NULL;
    FOR_EACH_RULE(i,rule) {
        
        if (!(rule->exec_flag & rq->query->rule_flag))
            continue;

        if (block->expanded && block->rule_id != rule->exec_flag) {
            continue;
        }
        if (block->iteration < rule->maxrec) {
            fs_rule *rule_copy = fs_rule_copy(rule,calln);
            fs_rule_bind_block *newb = 
                        fs_rule_bind_block_expand_with_rule(rq, block, rule_copy);
            if (newb) {
#ifdef DEBUG_RULES
                fs_error(LOG_ERR,"new block"); 
                fs_rule_bind_block_print(newb);
#endif
                newb->iteration = block->iteration + 1;
                block->expanded = 1;
                blocks_created++;
                rq->blocks[b++] = newb;
                newb->rule_id = rule->exec_flag;
            }
            fs_free_rule(rule_copy);
        }
    }

    return blocks_created;
}

void fs_rule_add_binding_to_query(fs_query *query, int block, fs_binding *binding) {
    fs_binding *vb = query->bb[block];
    for (int i=1; 1; i++) {
        if (!binding[i].name)
            break;
        for (int j=1; 1; j++) {
            if (!(vb+j)->name)
                break;
            if (!strcmp((vb+j)->name, binding[i].name)) {
                fs_rid_vector_append_vector((vb+j)->vals, binding[i].vals);
                (vb+j)->bound = 1;
                break;
            }
        }
    }
}

void fs_rule_extend_results(fs_rule_query *rule_query,rasqal_variable **vars,
                            fs_rid_vector ***results) {

    int count_slot = 0;
    for (int i=0;i<4;i++) {
        if (vars[i])
            count_slot++;
    }
    fs_rid_vector **rv = *results;
    if(!rv) {
        rv = calloc(count_slot, sizeof(fs_rid_vector *));
        for (int i=0;i<count_slot;i++)
            rv[i] = fs_rid_vector_new(0);
        *results = rv;
    }
    int bb=0;
    while (bb < QUERY_REWRITE_MAX_BLOCKS &&
       rule_query->blocks[bb]) {
       fs_binding *b = rule_query->blocks[bb]->result_binding;
       if (b) {
       for (int i=0;i<4;i++) {
            if (vars[i]) {
                int ib=0;
                int keep = 1;
                while (b[ib].name && keep) {
                    if (!strcmp(b[ib].name,(const char *)vars[i]->name)) {
                        fs_rid_vector_append_vector(rv[i], b[ib].vals);
                        keep = 0; 
                    }
                    ib++;
                }
            }
        }
        }
        bb++;
    }
}

fs_rule_bind_block *fs_rule_bind_block_from_rule_input(fs_rule_input *ri) {
    fs_rule_bind_block *block = calloc(1, sizeof(fs_rule_bind_block));
    fs_rule_bind *bind = calloc(1, sizeof(fs_rule_bind));
    bind->vrid_quad = fs_rule_copy_slots(ri->query_bind);
    bind->vars = fs_rule_copy_vars(ri->vars);
    block->binds[0] = bind;
    block->is_copy[0] = 0;
    block->iteration = 0;
    block->result_binding = NULL;
    return block;
}

fs_binding* fs_rule_bind_data_get(fs_query *query,fs_rule_bind *bind, double *elapse) {

#ifdef DEBUG_RULES
    GTimer *tmr = g_timer_new();
#endif

    fs_rid_vector *slot[4];
    for (int i=0;i<4; i++)
       slot[i] = fs_rid_vector_new(0);

    int tobind=0;
    int bind_comp[] = { FS_BIND_MODEL, FS_BIND_SUBJECT,
                     FS_BIND_PREDICATE, FS_BIND_OBJECT };

    int bind_cols=0;
    for (int q=0;q<4;q++) {
        if (bind->vars[q]) {
            tobind |= bind_comp[q];
            bind_cols++;
        }
        if (bind->vrid_quad[q] && bind->vrid_quad[q]->length > 0)
            fs_rid_vector_append_vector(slot[q], bind->vrid_quad[q]);
#if DEBUG_RULES > 2
        fs_error(LOG_ERR, "slot[%d]",q);
        fs_rid_vector_print(slot[q],1,stdout);
#endif
    }

    fs_binding *binding = NULL;
    int len = 0;
    if (!tobind) {
        goto tr_and_exit_bind;
    }

    int all = 1;
    if (fs_rid_vector_length(slot[3]) > 0)
        tobind |= FS_BIND_BY_OBJECT;
    else {
        tobind |= FS_BIND_BY_SUBJECT;
        all = !(fs_rid_vector_length(slot[1]) > 0);
    }

    fs_rid_vector **results = NULL;
    int ret = fs_bind_cache_wrapper(query->qs, query, all, tobind ,
         slot, &results, -1, query->order ? -1 : query->soft_limit);

    for (int i=0;i<4; i++)
        fs_rid_vector_free(slot[i]);

    if (ret) { 
        goto tr_and_exit_bind;
    }
    if (results) {
        len = results ? (results[0] ? results[0]->length : -1) : -2; 
    }
    if (len) {
        int r=0;
        binding = fs_binding_new();
        fs_binding_create(binding, "_ord", FS_RID_NULL, 0);
        for (int q=0;q<4;q++) {
            if (bind->vars[q] && results[r]) {
                fs_binding_get(binding,bind->vars[q]);
                fs_binding_add_vector(binding, bind->vars[q], results[r]);
                r++;
            }
        }
        if (results) {
            for (int q=0;q<bind_cols;q++) {
                if (results[q])
                    fs_rid_vector_free(results[q]);
            }
            free(results);

        }
    }
tr_and_exit_bind: ;
#ifdef DEBUG_RULES
    *elapse = g_timer_elapsed(tmr,NULL);
    g_timer_destroy(tmr);
#if DEBUG_RULES > 1
    fs_error(LOG_ERR,"fs_rule_bind_data_get elapse %.3f binding %d",
    *elapse,binding ? fs_binding_length(binding) : -1);
#endif
#endif
    return binding;
}

void fs_binding_set_sort(fs_binding *b, int value) {
    int ib = 0;
    while (b[ib].name) {
        b[ib++].sort = value;
    }
}

int fs_binding_set_equivalent_sort(fs_binding *a, fs_binding *b, int *ax, int *bx) {
    int ia = 1;
    int ib = 1;
    fs_binding_set_sort(a, 0); 
    fs_binding_set_sort(b, 0); 
    while (a[ia].name) {
        ib = 0;
        while (b[ib].name) {
            if (!strcmp(b[ib].name,a[ia].name)) {
                b[ib].sort = 1;
                a[ia].sort = 1;
                *ax = ia;
                *bx = ib;
                return 1;
            }
            ib++;
        }
        ia++;
    }
    return 0;
}

void fs_rule_binding_add_no_sort_columns(fs_binding *to, fs_binding *from, int *mappingto) {
    int i = 1; /* skip _ord */
    int ic = 0;

    while (to[ic].name) 
        ic++;
        
    while (from[i].name) {
        if (!from[i].sort) {
            to[ic].name = strdup(from[i].name);
            to[ic].bound = 1;
            to[ic].vals = fs_rid_vector_new(0);
            mappingto[i] = ic;
            ic++;
        } else {
            mappingto[i] = -1;
        }
        i++;
    }
}

fs_binding *fs_rule_binding_new_merge(fs_binding *a, fs_binding *b,char *first,
                                             int *mappinga, int *mappingb) {
    fs_binding *binding = fs_binding_new();
    fs_binding_create(binding, "_ord", FS_RID_NULL, 0);
    binding[1].bound = 1;
    binding[1].vals = fs_rid_vector_new(0);
    binding[1].name = strdup(first);
    fs_rule_binding_add_no_sort_columns(binding,a,mappinga);
    fs_rule_binding_add_no_sort_columns(binding,b,mappingb);
    return binding;
}


static inline void fs_rule_merge_add_row(fs_binding *to, fs_binding *from, int row, int *mapping, int width) {
   for (int i=1; i<width; i++) {
        if (mapping[i] > -1)
            fs_rid_vector_append((to+mapping[i])->vals,(from+i)->vals->data[row]);
   }
}

fs_binding *fs_rule_binding_merge(fs_binding *a, fs_binding *b, double *elapse) {
#ifdef DEBUG_RULES
    GTimer *tmr = g_timer_new();
#if DEBUG_RULES > 2
    fs_error(LOG_ERR,"merge bind a");
    fs_binding_print(a,stdout);
    fs_error(LOG_ERR,"merge bind b");
    fs_binding_print(b,stdout);
#endif 
#endif

    int ax, bx;
    int match = fs_binding_set_equivalent_sort(a, b, &ax, &bx);
    if (!match)
        return NULL;

    fs_binding *abx = a+ax;
    fs_binding *bbx = b+bx;

    fs_binding_sort(a);
    fs_binding_sort(b);
    
    int wa = fs_binding_width(a);
    int wb = fs_binding_width(b);
    int mappinga[wa];
    for (int i=0; i<wa; i++)
        mappinga[i] = -1;
        
    int mappingb[wb];
    for (int i=0; i<wb; i++)
        mappingb[i] = -1;
        
    fs_binding *c = fs_rule_binding_new_merge(a,b,abx->name,mappinga, mappingb);

    int vb = 0;
    fs_rid arid = FS_RID_NULL;
    fs_rid brid = FS_RID_NULL;
    for (int va=0; va < abx->vals->length; va++) {
        arid = abx->vals->data[a->vals->data[va]];
        if (arid >= brid) {
            while (vb < bbx->vals->length) {
                brid = bbx->vals->data[b->vals->data[vb]];
                if (arid == brid) {
                    fs_rule_merge_add_row(c,a,a->vals->data[va],mappinga,wa);
                    fs_rule_merge_add_row(c,b,b->vals->data[vb],mappingb,wb);
                    fs_rid_vector_append((c+1)->vals,arid);
                } else if (brid > arid) {
                    break;
                }
                vb++;
            }
        }
    }
#ifdef DEBUG_RULES
    *elapse = g_timer_elapsed(tmr,NULL);
    g_timer_destroy(tmr);
#if DEBUG_RULES > 2
    fs_error(LOG_ERR,"merge bind result c in %.3f",*elapse);
    fs_binding_print(c,stdout);
#endif 
#endif 
    return c;
}

int count_constants_in_bind(fs_rule_bind *bind) {
    int c=0;
    for (int i=0; i < 4; i++) {
        if (bind->vrid_quad[i] &&
                bind->vrid_quad[i]->length > 0) {
            if (i == 2)
                c += 5;
            else if (i == 3)
                c += 2;
            else if (i == 1)
                c += 2;
            else if (i == 0)
                c++;
        }
    }
    return c++;
}

int get_bind_backwards(fs_rule_bind_block *rb) { 
    fs_rule_bind *b_first = rb->binds[0];
    int bi =QUERY_REWRITE_MAX_BIND_PER_BLOCK;
    while(bi > 0 
            &&  rb->binds[bi] == NULL)
           bi--;

    if (!bi)
        return 0;

    fs_rule_bind *b_last = rb->binds[bi];

    return count_constants_in_bind(b_last) >
             count_constants_in_bind(b_first) ? 1 : 0;         
}

fs_binding *fs_rule_bind_block_process_direction(fs_query *query,
                                  fs_rule_bind_block *rb,
                                  int bi,
                                  fs_binding *prev_binding) {

   fs_rule_bind *bind = rb->binds[bi];

   if (prev_binding && !fs_binding_length(prev_binding)) {
       fs_binding_free(prev_binding);
       return NULL;
   }

   double elapse_bind_data;
   fs_binding *binding = fs_rule_bind_data_get(query, bind, &elapse_bind_data);
#ifdef DEBUG_RULES
        rb->elapse_data_binds += elapse_bind_data;
#endif
   if (!binding) {
        if (prev_binding)
            fs_binding_free(prev_binding);
        return NULL;
   }
   if (!prev_binding) {
       prev_binding = binding;
   } else {
       double elapse_merge;
       fs_binding *merged = fs_rule_binding_merge(prev_binding, binding, &elapse_merge); 
#ifdef DEBUG_RULES
        rb->elapse_merges += elapse_merge;
#endif
       fs_binding_free(binding);
       if (prev_binding)
           fs_binding_free(prev_binding);
       prev_binding = merged;
   }
#if DEBUG_RULES > 0
   if (prev_binding)
        fs_error(LOG_ERR,"fs_rule_bind_block_process [%d] merged items %d",
            bi, fs_binding_length(prev_binding));
#endif
    return prev_binding;
}


fs_binding *fs_rule_bind_block_process(fs_query *query, fs_rule_bind_block *rb) {
#ifdef DEBUG_RULES
    GTimer *tmr = g_timer_new();
#endif

   int bind_backwards = get_bind_backwards(rb);
#ifdef DEBUG_RULES
   fs_error(LOG_ERR,"bind_backwards %d",bind_backwards);
#endif
   fs_binding *prev_binding=NULL;
   if (bind_backwards) {
       int bi = QUERY_REWRITE_MAX_BIND_PER_BLOCK;

       while(bi >= 0 &&  rb->binds[bi] == NULL)
           bi--;
       if (bi < 0)
           goto tr_exit_bd;
       while(bi >= 0 && rb->binds[bi] != NULL) {
           prev_binding = 
               fs_rule_bind_block_process_direction(query,rb,bi,prev_binding);
           if (!prev_binding)
               goto tr_exit_bd;
           bi--;
       }
   } else {
       int bi = 0;
       while(bi < QUERY_REWRITE_MAX_BIND_PER_BLOCK &&
             rb->binds[bi] != NULL) {
           prev_binding = 
               fs_rule_bind_block_process_direction(query,rb,bi,prev_binding);
           if (!prev_binding)
               goto tr_exit_bd;
           bi++;
       }
   }

tr_exit_bd: ;
#ifdef DEBUG_RULES
    double elapse = g_timer_elapsed(tmr,NULL);
    rb->elapse_total = elapse;
    g_timer_destroy(tmr);
    fs_error(LOG_ERR,"fs_rule_bind_block_process rule_id[%x] elapse (%.3f,%.3f,%.3f)",
                      rb->rule_id,
                      rb->elapse_merges,rb->elapse_data_binds,rb->elapse_total);
#endif
   return prev_binding;
}

fs_rule_query * fs_rule_create_query(fs_query *query, int block, fs_rule_input *rule_input) {
    fs_rule_context_init_sync(query);

    fs_rule_query *rule_q = calloc(1, sizeof(fs_rule_query));
    rule_q->count = 0;
    rule_q->block = block;
    rule_q->rule_input = rule_input;
    rule_q->query = query;
    for (int i=0;i<QUERY_REWRITE_MAX_BLOCKS;i++)
        rule_q->blocks[i] = NULL;

    fs_rule_bind_block *firstb = fs_rule_bind_block_from_rule_input(rule_input);
    rule_q->blocks[0] = firstb;
    firstb->rule_id = -1;
    int total_blocks = 1;
    int i = 0;
    int finish = 0;

#ifdef DEBUG_RULES
    fs_error(LOG_ERR,"slots in create rule query:");
    for (int x=0;x<4;x++) {
        if (rule_input->query_bind[x])
            fs_rid_vector_print(rule_input->query_bind[x],1,stdout);
    }
#endif

    while(!finish) {
        int nb = fs_rule_expand_bind_block_expand(rule_q, rule_q->blocks[i], i);
        total_blocks += nb;
        i++;
        finish = rule_q->blocks[i] == NULL;
    }
   
    /* we skip postition 0 .. it is the query */
    /* TODO: skip iterate should not hardcoded to just SUBC */
    int skip_iterate = 0x0002;

    int skip_flag = 0x0;
    for (int b=1; b < i; b++) {
        if (skip_flag & rule_q->blocks[b]->rule_id)
            continue;
#if DEBUG_RULES > 0
        fs_error(LOG_ERR,"Executing bind for block %d",b);
#endif
        fs_binding *binding = 
            fs_rule_bind_block_process(query,rule_q->blocks[b]);

        if ((!binding || !fs_binding_length(binding))
            && (rule_q->blocks[b]->rule_id & skip_iterate)) {
            skip_flag |= rule_q->blocks[b]->rule_id;
#ifdef DEBUG_RULES
            fs_error(LOG_ERR,"Next attempts to run rule %d will be skipped",
                                rule_q->blocks[b]->rule_id);
            fs_error(LOG_ERR,"cause %p %d",binding,
                                binding ? fs_binding_length(binding) : -1);
#endif
        }

        if (binding && fs_binding_length(binding) > 0) {
            rule_q->count += fs_binding_length(binding);
            rule_q->blocks[b]->result_binding = binding;
        } else if (binding) {
            fs_binding_free(binding);
        }
    }
    return rule_q;
}


int fs_rule_flags_from_string(const char *srules) {
    if (!srules)
        return 0;
    if (strstr(srules,"NONE"))
        return 0;

    if (!strcmp(srules, "DEFAULT"))
        return 0x0001; /* only subp */
    int flag = 0;
    if (strstr(srules,"SUBP"))
        flag |= 0x0001;
    if (strstr(srules,"SUBC"))
        flag |= 0x0002;
    return flag;
}
