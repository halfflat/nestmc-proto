#include <iomanip>
#include <iostream>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

#include <util/optional.hpp>

#include "symbolic.hpp"
#include "msparse.hpp"

using namespace sym;

// identifier name picking helper routines

void join_impl(std::ostream& ss, const std::string& sep) {}

template <typename Head, typename... Args>
void join_impl(std::ostream& ss, const std::string& sep, Head&& head, Args&&... tail) {
    if (sizeof...(tail)==0)
        ss << std::forward<Head>(head);
    else
        join_impl(ss << std::forward<Head>(head) << sep, sep, std::forward<Args>(tail)...);
}

template <typename... Args>
std::string join(const std::string& sep, Args&&... items) {
    std::stringstream ss;
    join_impl(ss, sep, std::forward<Args>(items)...);
    return ss.str();
}

class id_maker {
private:
    std::unordered_set<std::string> ids;

public:
    static std::string next_id(std::string s) {
        unsigned l = s.size();
        if (l==0) return "a";

        unsigned i = l-1;

        char l0='a', l1='z';
        char u0='A', u1='Z';
        char d0='0', d1='9';
        for (;;) {
            char& c = s[i];
            if ((c>=l0 && c<l1) || (c>=u0 && c<u1) || (c>=d0 && c<d1)) {
                ++c;
                return s;
            }
            if (c==l1) c=l0;
            if (c==u1) c=u0;
            if (c==d1) c=d0;
            if (i==0) break;
            --i;
        }

        // prepend a character based on class of first
        if (s[0]==u0) return u0+s;
        if (s[0]==d0) return d0+s;
        return l0+s;
    }

    template <typename... Args>
    std::string operator()(Args&&... elements) {
        std::string name = join("",std::forward<Args>(elements)...);
        if (name.empty()) name = "a";

        while (ids.count(name)) {
            name = next_id(name);
        }
        ids.insert(name);
        return name;
    }

    void reserve(std::string name) {
        ids.insert(std::move(name));
    }
};

// ostream output functions

template <typename X>
std::ostream& operator<<(std::ostream& o, const optional<X>& x) {
    return x? o << *x: o << "nothing";
}

template <typename X>
std::ostream& operator<<(std::ostream& o, const msparse::matrix<X>& m) {
    for (unsigned r = 0; r<m.nrow(); ++r) {
        o << '|';
        for (unsigned c = 0; c<m.ncol(); ++c) {
            o << std::setw(12) << m[r][c];
        }
        o << " |\n";
    }
    return o;
}

template <typename Sep, typename V>
struct sepval_t {
    const Sep& sep;
    const V& v;

    sepval_t(const Sep& sep, const V& v): sep(sep), v(v) {}

    friend std::ostream& operator<<(std::ostream& O, const sepval_t& sv) {
        bool first = true;
        for (const auto& x: sv.v) {
            if (!first) O << sv.sep;
            first = false;
            O << x;
        }
        return O;
    }
};

template <typename Sep, typename V>
sepval_t<Sep,V> sepval(const Sep& sep, const V& v) { return sepval_t<Sep,V>(sep, v); }

std::ostream& operator<<(std::ostream& o, const symbol_table& syms) {
    for (unsigned i = 0; i<syms.size(); ++i) {
        symbol s = syms[i];
        o << s;
        if (s.def()) o << ": " << s.def();
        o << "\n";
    }
    return o;
}

// symbolic GE

using symmrow = msparse::mrow<symbol>;
using symmatrix = msparse::matrix<symbol>;

// return q[c]*p - p[c]*q
template <typename DefineSym>
symmrow row_reduce(unsigned c, const symmrow& p, const symmrow& q, DefineSym define_sym) {
    if (p.index(c)==p.npos || q.index(c)==q.npos) throw std::runtime_error("improper row GE");

    symmrow u;
    symbol x = q[c];
    symbol y = p[c];

    auto piter = p.begin();
    auto qiter = q.begin();
    unsigned pj = piter->first;
    unsigned qj = qiter->first;

    while (piter!=p.end() || qiter!=q.end()) {
        unsigned j = std::min(pj, qj);
        symbol_term t1, t2;

        if (j==pj) {
            t1 = x*piter->second;
            ++piter;
            pj = piter==p.end()? p.npos: piter->first;
        }
        if (j==qj) {
            t1 = y*qiter->second;
            ++qiter;
            qj = qiter==q.end()? q.npos: qiter->first;
        }
        if (j!=c) {
            u.push_back({j, define_sym(t1-t2)});
        }
    }
    return u;
}

// ncol: number of columns before augmentation
template <typename DefineSym>
void gj_reduce(symmatrix& A, unsigned ncol, DefineSym define_sym) {
    struct pq_entry {
        unsigned key;
        unsigned mincol;
        unsigned row;
    };

    struct pq_order_t {
        bool operator()(const pq_entry& a, const pq_entry& b) const {
            return a.key>b.key || (a.key==b.key && a.mincol<b.mincol);
        }
    };

    std::priority_queue<pq_entry, std::vector<pq_entry>, pq_order_t> pq;

    unsigned n = A.nrow();
    for (unsigned i = 0; i<n; ++i) {
        unsigned c = A[i].mincol();
        if (c<ncol) pq.push({c, c, i});
    }

    while (!pq.empty()) {
        pq_entry pick = pq.top();
        pq.pop();

        unsigned col = pick.key;
        auto r1 = pick.row;

        while (!pq.empty() && pq.top().key==pick.key) {
            pq_entry top = pq.top();
            pq.pop();

            auto r2 = top.row;
            A[r2] = row_reduce(col, A[r2], A[r1], define_sym);

            unsigned c = A[r2].mincol_after(col);
            if (c<ncol) pq.push({c, A[r2].mincol(), r2});
        }
        unsigned c = A[r1].mincol_after(col);
        if (c<ncol) pq.push({c, pick.mincol, r1});
    }
}

// functionality demo/tests

template <typename Rng>
msparse::matrix<double> make_random_matrix(unsigned n, double density, Rng& R) {
    std::uniform_real_distribution<double> U;
    msparse::matrix<double> M(n, n);

    for (unsigned i = 0; i<n; ++i) {
        for (unsigned j = 0; j<n; ++j) {
            if (i!=j && U(R)>density) continue;
            double u = U(R);
            M[i][j] = i==j? n*(1+u): u-0.5;
        }
    }
    return M;
}

void demo_msparse_random() {
    std::minstd_rand R;
    msparse::matrix<double> M = make_random_matrix(5, 0.3, R);

    std::cout << "M:\n" << M;

    int x[] = { 1, 2, 3, 4, 5 };
    std::cout << "x: " << sepval(',', x) << "\n";

    std::vector<double> b(5);
    mul_dense(M, x, b);
    std::cout << "Mx: " << sepval(',', b) << "\n";
}

void demo_store_eval() {
    symbol_table syms;
    store vals(syms);

    auto a1 = syms.define("a1");
    auto a2 = syms.define("a2");
    auto a3 = syms.define("a3");
    auto b  = syms.define("b", a1*a2-a2*a3);
    auto c  = syms.define("c", a1*a2-a1*b);
    auto d  = syms.define("d", -(a3*c));

    std::cout << syms;

    vals[a1] = 2;
    vals[a2] = 3;
    vals[a3] = 5;

    std::cout << d << "=" << vals.evaluate(d) << "\n";

    std::cout << "value store\n";
    for (unsigned i = 0; i<syms.size(); ++i) {
        symbol s = syms[i];
        std::cout << s << "=" << vals[s] << "\n";
    }
}

void demo_sym_ge() {
    std::minstd_rand R;
    unsigned n = 5;
    msparse::matrix<double> M = make_random_matrix(n, 0.3, R);

    symbol_table syms;
    store vals(syms);
    id_maker make_id;
    symmatrix S(n, n);

    for (unsigned i = 0; i<M.nrow(); ++i) {
        const auto& row = M.rows[i];
        symmrow r;
        for (const auto& el: row) {
            unsigned j = el.first;
            auto a = syms.define(make_id("a", i, j));
            vals[a] = el.second;
            r.push_back({j, a});
        }
        S.rows[i] = r;
    }

    std::cout << "M:\n" << M;
    std::cout << "S:\n" << S;

    gj_reduce(S, n, [&](const symbol_def& def) { return syms.define(make_id(), def); });
    std::cout << "S:\n" << S;

    std::cout << "symbols:\n" << syms;
}


int main() {
    // demo_store_eval();
    // demo_msparse_random();
    demo_sym_ge();
}
