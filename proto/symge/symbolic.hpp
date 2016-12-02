#pragma once

#include <iosfwd>
#include <string>
#include <stdexcept>
#include <vector>

#include <util/optional.hpp>

namespace sym {

template <typename X>
using optional = nest::mc::util::optional<X>;

using nest::mc::util::nothing;
using nest::mc::util::just;

struct symbol_error: public std::runtime_error {
    symbol_error(const std::string& what): std::runtime_error(what) {}
};

// Symbols are either primitive or represent the difference
// between two product terms.

template <typename S>
struct symbol_term_ {
    S a, b;

    symbol_term_() = default;

    // true iff representing non-zero
    operator bool() const {
        return a && b;
    }
};

template <typename S>
struct symbol_term_diff_ {
    symbol_term_<S> left, right;

    symbol_term_diff_() = default;
    symbol_term_diff_(const symbol_term_<S>& left): left(left), right{} {}
    symbol_term_diff_(const symbol_term_<S>& left, const symbol_term_<S>& right):
       left(left), right(right) {}
};

template <typename S>
using symbol_def_ = optional<symbol_term_diff_<S>>;

template <typename S>
class symbol_table_ {
public:
    struct table_entry {
        std::string name;
        symbol_def_<S> def;
    };

    S define(const std::string& name, const symbol_def_<S>& definition = nothing) {
        unsigned idx = size();
        entries.push_back({name, definition});
        return S{idx, this};
    }

    S operator[](unsigned i) const {
        if (i>=size()) throw symbol_error("no such symbol");
        return S{i, this};
    }

    std::size_t size() const {
        return entries.size();
    }

    const symbol_def_<S>& def(S s) const {
        if (!valid(s)) throw symbol_error("symbol not present in this table");
        return entries[s.idx].def;
    }

    const std::string& name(S s) const {
        if (!valid(s)) throw symbol_error("symbol not present in this table");
        return entries[s.idx].name;
    }

private:
    std::vector<table_entry> entries;
    bool valid(S s) const {
        return s.tbl==this && s.idx<entries.size();
    }
};

class symbol {
private:
    friend class symbol_table_<symbol>;
    unsigned idx = 0;
    const symbol_table_<symbol>* tbl = nullptr;

    symbol(unsigned idx, const symbol_table_<symbol>* tbl):
        idx(idx), tbl(tbl) {}

public:
    using symbol_table = symbol_table_<symbol>;
    using symbol_def = symbol_def_<symbol>;

    symbol() = default;
    symbol(const symbol&) = default;
    symbol& operator=(const symbol&) = default;

    operator bool() const { return (bool)tbl; }

    std::string str() const {
        return tbl? tbl->name(*this): "";
    }

    symbol_def def() const {
        return tbl? tbl->def(*this): symbol_def{};
    }

    bool primitive() const { return (bool)def(); }

    optional<unsigned> index(const symbol_table& in_table) const {
        return tbl==&in_table? just(idx): nothing;
    }
};

using symbol_term = symbol_term_<symbol>;
using symbol_term_diff = symbol_term_diff_<symbol>;
using symbol_def = symbol_def_<symbol>;
using symbol_table = symbol_table_<symbol>;

inline symbol_term_diff operator-(const symbol_term& left, const symbol_term& right) {
    return symbol_term_diff{left, right};
}

inline symbol_term_diff operator-(const symbol_term& right) {
    return symbol_term_diff{symbol_term{}, right};
}

inline symbol_term operator*(const symbol& a, const symbol& b) {
    return symbol_term{a, b};
}

inline std::ostream& operator<<(std::ostream& o, const symbol& s) {
    return o << s.str();
}

inline std::ostream& operator<<(std::ostream& o, const symbol_term& term) {
    if (term) return o << term.a.str() << '*' << term.b.str();
    else return o << '0';
}

inline std::ostream& operator<<(std::ostream& o, const symbol_term_diff& diff) {
    if (!diff.right) return o << diff.left;
    else {
        if (diff.left) o << diff.left;
        o << '-';
        return o << diff.right;
    }
}

// A store represents map from symbols (from one table) to values.

class store {
private:
    const symbol_table& table;
    std::vector<optional<double>> data;

public:
    explicit store(const symbol_table& table): table(table) {}

    optional<double>& operator[](const symbol& s) {
        if (auto idx = s.index(table)) {
            if (*idx>=data.size()) data.resize(1+*idx);
            return data[*idx];
        }
        throw symbol_error("symbol not associated with store table");
    }

    optional<double> operator[](const symbol& s) const {
        auto idx = s.index(table);
        if (idx && *idx<data.size()) return data[*idx];
        else return nothing;
    }

    optional<double> evaluate(const symbol& s) {
        auto& value = (*this)[s];
        if (!value) {
            if (auto def = s.def()) {
                value = evaluate(*def);
            }
        }
        return value;
    }

    optional<double> evaluate(const symbol_term& t) {
        if (!t) return 0;
        auto a = evaluate(t.a);
        auto b = evaluate(t.b);
        return a && b? just((*a)*(*b)): nothing;
    }

    optional<double> evaluate(const symbol_term_diff& d) {
        auto l = evaluate(d.left);
        auto r = evaluate(d.right);
        return l && r? just((*l)-(*r)): nothing;
    }
};

} // namespace sym

