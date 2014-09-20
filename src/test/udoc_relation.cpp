#include "udoc_relation.h"
#include "trace.h"
#include "vector.h"
#include "ast.h"
#include "ast_pp.h"
#include "reg_decl_plugins.h"
#include "sorting_network.h"
#include "smt_kernel.h"
#include "model_smt2_pp.h"
#include "smt_params.h"
#include "ast_util.h"
#include "expr_safe_replace.h"
#include "th_rewriter.h"
#include "dl_relation_manager.h"
#include "dl_register_engine.h"
#include "rel_context.h"
#include "bv_decl_plugin.h"

class udoc_tester {
    typedef datalog::relation_base relation_base;
    typedef datalog::udoc_relation udoc_relation;
    typedef datalog::udoc_plugin   udoc_plugin;
    typedef datalog::relation_signature relation_signature;
    typedef datalog::relation_fact relation_fact;
    typedef scoped_ptr<datalog::relation_mutator_fn> rel_mut;
    typedef scoped_ptr<datalog::relation_union_fn> rel_union;

    struct init {
        init(ast_manager& m) {
            reg_decl_plugins(m);
        }
    };
    random_gen                m_rand;
    ast_manager               m;
    init                      m_init;
    bv_util                   bv;
    expr_ref_vector           m_vars;
    smt_params                m_smt_params;
    datalog::register_engine  m_reg;
    datalog::context          m_ctx;
    datalog::rel_context      rc;
    udoc_plugin&              p;
public:
    udoc_tester(): m_init(m), bv(m), m_vars(m), m_ctx(m, m_reg, m_smt_params), rc(m_ctx),
                   p(dynamic_cast<udoc_plugin&>(*rc.get_rmanager().get_relation_plugin(symbol("doc"))))
    {        
    }

    udoc_relation* mk_empty(relation_signature const& sig) {
        SASSERT(p.can_handle_signature(sig));
        relation_base* empty = p.mk_empty(sig);
        return dynamic_cast<udoc_relation*>(empty);
    }

    udoc_relation* mk_full(relation_signature const& sig) {
        func_decl_ref fn(m);
        fn = m.mk_func_decl(symbol("full"), sig.size(), sig.c_ptr(), m.mk_bool_sort());
        relation_base* full = p.mk_full(fn, sig);
        return dynamic_cast<udoc_relation*>(full);
    }

    void test1() {
        datalog::relation_signature sig;
        sig.push_back(bv.mk_sort(12));
        sig.push_back(bv.mk_sort(6));
        sig.push_back(bv.mk_sort(12));
        

        datalog::relation_fact fact1(m), fact2(m), fact3(m);
        fact1.push_back(bv.mk_numeral(rational(1), 12));
        fact1.push_back(bv.mk_numeral(rational(6), 6));
        fact1.push_back(bv.mk_numeral(rational(56), 12));
        fact2.push_back(bv.mk_numeral(rational(8), 12));
        fact2.push_back(bv.mk_numeral(rational(16), 6));
        fact2.push_back(bv.mk_numeral(rational(32), 12));
        fact3.push_back(bv.mk_numeral(rational(32), 12));
        fact3.push_back(bv.mk_numeral(rational(16), 6));
        fact3.push_back(bv.mk_numeral(rational(4), 12));

        relation_signature sig2;
        sig2.push_back(bv.mk_sort(3));
        sig2.push_back(bv.mk_sort(6));
        sig2.push_back(bv.mk_sort(3));
        sig2.push_back(bv.mk_sort(3));
        sig2.push_back(bv.mk_sort(3));

        relation_base* t;
        udoc_relation* t1, *t2, *t3;
        expr_ref fml(m);
        // empty
        {
            std::cout << "empty\n";
            t = mk_empty(sig);
            t->display(std::cout); std::cout << "\n";
            t->to_formula(fml);
            std::cout << fml << "\n";
            t->deallocate();
        }

        // full
        {
            std::cout << "full\n";
            t = mk_full(sig);
            t->display(std::cout); std::cout << "\n";
            t->to_formula(fml);
            std::cout << fml << "\n";
            t->deallocate();
        }

        // join
        {
            t1 = mk_full(sig);
            t2 = mk_full(sig);
            t3 = mk_empty(sig);
            unsigned_vector jc1, jc2;
            jc1.push_back(1);
            jc2.push_back(1);
            datalog::relation_join_fn* join_fn = p.mk_join_fn(*t1, *t2, jc1.size(), jc1.c_ptr(), jc2.c_ptr());
            SASSERT(join_fn);
            t = (*join_fn)(*t1, *t2);
            t->display(std::cout); std::cout << "\n";
            t->deallocate();

            t = (*join_fn)(*t1, *t3);
            SASSERT(t->empty());
            t->display(std::cout); std::cout << "\n";
            t->deallocate();

            t = (*join_fn)(*t3, *t3);
            SASSERT(t->empty());
            t->display(std::cout); std::cout << "\n";
            t->deallocate();
                       
            dealloc(join_fn);
            t1->deallocate();
            t2->deallocate();
            t3->deallocate();
        }

        // project
        {
            std::cout << "project\n";
            t1 = mk_full(sig);
            unsigned_vector pc;
            pc.push_back(0);
            datalog::relation_transformer_fn* proj_fn = p.mk_project_fn(*t1, pc.size(), pc.c_ptr());
            t = (*proj_fn)(*t1);
            t->display(std::cout); std::cout << "\n";
            t->deallocate();
            
            t1->reset();
            t = (*proj_fn)(*t1);
            t->display(std::cout); std::cout << "\n";
            t->deallocate();
            
            t1->add_fact(fact1);
            t1->add_fact(fact2);
            t1->add_fact(fact3);
            t = (*proj_fn)(*t1);
            t1->display(std::cout); std::cout << "\n";
            t->display(std::cout); std::cout << "\n";
            t->deallocate();

            dealloc(proj_fn);
            t1->deallocate();
        }

        // rename
        {
            t1 = mk_empty(sig);
            unsigned_vector cycle;
            cycle.push_back(0);
            cycle.push_back(2);
            datalog::relation_transformer_fn* rename = p.mk_rename_fn(*t1, cycle.size(), cycle.c_ptr());

            t1->add_fact(fact1);
            t1->add_fact(fact2);
            t1->add_fact(fact3);
            t = (*rename)(*t1);
            t1->display(std::cout); std::cout << "\n";
            t->display(std::cout); std::cout << "\n";
            t->deallocate();
            
            dealloc(rename);
            t1->deallocate();
        }

        // union
        {
            t1 = mk_empty(sig);
            t2 = mk_empty(sig);
            udoc_relation* delta = mk_full(sig);
            t2->add_fact(fact1);
            t2->add_fact(fact2);
            t1->add_fact(fact3);

            rel_union union_fn = p.mk_union_fn(*t1, *t2, 0);

            t1->display(std::cout << "t1 before:"); std::cout << "\n";
            (*union_fn)(*t1, *t2, delta);
            t1->display(std::cout << "t1 after:"); std::cout << "\n";
            delta->display(std::cout << "delta:"); std::cout << "\n";
            
            t1->deallocate();
            t2->deallocate();
            delta->deallocate();
        }

        // filter_identical
        {
            t1 = mk_empty(sig2);
            unsigned_vector id;
            id.push_back(0);
            id.push_back(2);
            id.push_back(4);
            datalog::relation_mutator_fn* filter_id = p.mk_filter_identical_fn(*t1, id.size(), id.c_ptr());
            relation_fact f1(m);
            f1.push_back(bv.mk_numeral(rational(1),3));
            f1.push_back(bv.mk_numeral(rational(1),6));
            f1.push_back(bv.mk_numeral(rational(1),3));
            f1.push_back(bv.mk_numeral(rational(1),3));
            f1.push_back(bv.mk_numeral(rational(1),3));
            t1->add_fact(f1);
            f1[4] = bv.mk_numeral(rational(2),3);
            t1->add_fact(f1);
            t1->display(std::cout); std::cout << "\n";
            (*filter_id)(*t1);
            t1->display(std::cout); std::cout << "\n";
            t1->deallocate();
            dealloc(filter_id);
        }

        // filter_interpreted
        {
            std::cout << "filter interpreted\n";
            t1 = mk_full(sig2);
            t2 = mk_full(sig2);
            var_ref v0(m.mk_var(0, bv.mk_sort(3)),m);
            var_ref v1(m.mk_var(1, bv.mk_sort(6)),m);
            var_ref v2(m.mk_var(2, bv.mk_sort(3)),m);
            var_ref v3(m.mk_var(3, bv.mk_sort(3)),m);
            var_ref v4(m.mk_var(4, bv.mk_sort(3)),m);
            app_ref cond1(m), cond2(m), cond3(m), cond4(m);
            app_ref cond5(m), cond6(m), cond7(m), cond8(m);
            cond1 = m.mk_true();
            cond2 = m.mk_false();
            cond3 = m.mk_eq(v0, v2);
            cond4 = m.mk_not(m.mk_eq(v0, v2));
            cond5 = m.mk_eq(v0, bv.mk_numeral(rational(2), 3));

            rel_union union_fn = p.mk_union_fn(*t1, *t2, 0);
            rel_mut fint1 = p.mk_filter_interpreted_fn(*t1, cond1);
            rel_mut fint2 = p.mk_filter_interpreted_fn(*t1, cond2);
            rel_mut fint3 = p.mk_filter_interpreted_fn(*t1, cond3);
            rel_mut fint4 = p.mk_filter_interpreted_fn(*t1, cond4);
            rel_mut fint5 = p.mk_filter_interpreted_fn(*t1, cond5);
            (*fint1)(*t1);
            t1->display(std::cout << "filter: " << cond1 << " "); std::cout << "\n";
            (*fint2)(*t1);
            t1->display(std::cout << "filter: " << cond2 << " "); std::cout << "\n";
            (*union_fn)(*t1, *t2); 
            (*fint3)(*t1);
            t1->display(std::cout << "filter: " << cond3 << " "); std::cout << "\n";
            (*fint4)(*t1);
            t1->display(std::cout << "filter: " << cond4 << " "); std::cout << "\n";
            (*union_fn)(*t1, *t2); 
            (*fint5)(*t1);
            t1->display(std::cout << "filter: " << cond5 << " "); std::cout << "\n";                        
            t1->deallocate();
            t2->deallocate();

        }
        // filter_by_negation

        // filter_interpreted_project
    }
};

void tst_udoc_relation() {
    udoc_tester tester;

    tester.test1();
}
