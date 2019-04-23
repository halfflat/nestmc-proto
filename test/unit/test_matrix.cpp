#include <numeric>
#include <vector>

#include "../gtest.h"

#include <arbor/math.hpp>

#include "matrix.hpp"
#include "backends/multicore/fvm.hpp"
#include "util/rangeutil.hpp"
#include "util/span.hpp"

#include "common.hpp"

using namespace arb;

using matrix_type = matrix<arb::multicore::backend>;
using index_type = matrix_type::index_type;
using value_type = matrix_type::value_type;

using ivec = std::vector<index_type>;
using vvec = std::vector<value_type>;

TEST(matrix, construct_from_parent_only)
{
    ivec p = {0,0,1};
    matrix_type m(p, {0, 3}, vvec(3), vvec(3), vvec(3), {0});
    EXPECT_EQ(m.num_cells(), 1u);
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(p.size(), 3u);

    auto mp = m.p();
    EXPECT_EQ(mp[0], index_type(0));
    EXPECT_EQ(mp[1], index_type(0));
    EXPECT_EQ(mp[2], index_type(1));
}

TEST(matrix, solve_host)
{
    using util::make_span;
    using util::fill;

    // trivial case : 1x1 matrix
    {
        matrix_type m({0}, {0,1}, vvec(1), vvec(1), vvec(1), {0});
        auto& state = m.state_;
        fill(state.d,  2);
        fill(state.u, -1);
        fill(state.rhs,1);

        m.solve();

        EXPECT_EQ(m.solution()[0], 0.5);
    }

    // matrices in the range of 2x2 to 1000x1000
    {
        for(auto n : make_span(2, 1001)) {
            auto p = ivec(n);
            std::iota(p.begin()+1, p.end(), 0);
            matrix_type m(p, {0, n}, vvec(n), vvec(n), vvec(n), {0});

            EXPECT_EQ(m.size(), (unsigned)n);
            EXPECT_EQ(m.num_cells(), 1u);

            auto& A = m.state_;

            fill(A.d,  2);
            fill(A.u, -1);
            fill(A.rhs,1);

            m.solve();

            auto x = m.solution();
            auto err = math::square(std::fabs(2.*x[0] - x[1] - 1.));
            for(auto i : make_span(1,n-1)) {
                err += math::square(std::fabs(2.*x[i] - x[i-1] - x[i+1] - 1.));
            }
            err += math::square(std::fabs(2.*x[n-1] - x[n-2] - 1.));

            EXPECT_NEAR(0., std::sqrt(err), 1e-8);
        }
    }
}

TEST(matrix, zero_diagonal)
{
    // Combined matrix may have zero-blocks, corresponding to a zero dt.
    // Zero-blocks are indicated by zero value in the diagonal (the off-diagonal
    // elements should be ignored).
    // These submatrices should leave the rhs as-is when solved.

    using util::assign;

    // Three matrices, sizes 3, 3 and 2, with no branching.
    ivec p = {0, 0, 1, 3, 3, 5, 5};
    ivec c = {0, 3, 5, 7};
    ivec i = {0, 1, 2};
    matrix_type m(p, c, vvec(7), vvec(7), vvec(7), i);

    EXPECT_EQ(7u, m.size());
    EXPECT_EQ(3u, m.num_cells());

    auto& A = m.state_;
    assign(A.d,   vvec({2,  3,  2, 0,  0,  4,  5}));
    assign(A.u,   vvec({0, -1, -1, 0, -1,  0, -2}));
    assign(A.rhs, vvec({3,  5,  7, 7,  8, 16, 32}));

    // Expected solution:
    std::vector<value_type> expected = {4, 5, 6, 7, 8, 9, 10};

    m.solve();
    auto x = m.solution();

    EXPECT_TRUE(testing::seq_almost_eq<double>(expected, x));
}

TEST(matrix, zero_diagonal_assembled)
{
    // Use assemble method to construct same zero-diagonal
    // test case from CV data.

    using util::assign;
    using array = matrix_type::array;

    // Combined matrix may have zero-blocks, corresponding to a zero dt.
    // Zero-blocks are indicated by zero value in the diagonal (the off-diagonal
    // elements should be ignored).
    // These submatrices should leave the rhs as-is when solved.

    // Three matrices, sizes 3, 3 and 2, with no branching.
    ivec p = {0, 0, 1, 3, 3, 5, 5};
    ivec c = {0, 3, 5, 7};
    ivec s = {0, 1, 2};

    // Face conductances.
    vvec g = {0, 1, 1, 0, 1, 0, 2};

    // dt of 1e-3.
    array dt(3, 1.0e-3);

    // Capacitances.
    vvec Cm = {1, 1, 1, 1, 1, 2, 3};

    // Intial voltage of zero; currents alone determine rhs.
    array v(7, 0.0);
    vvec area(7, 1.0);

    // (Scaled) membrane conductances contribute to diagonal.
    array mg = { 1000, 2000, 3000, 4000, 5000, 6000, 7000 };
    array i = {-7000, -15000, -25000, -34000, -49000, -70000, -102000};

    // Expected matrix and rhs:
    // u   = [ 0 -1 -1  0 -1  0 -2]
    // d   = [ 3  5  5  6  7  10  12]
    // rhs = [ 7 15 25 34 49 70 102 ]
    //
    // Expected solution:
    // x = [ 4 5 6 7 8 9 10 ]

    matrix_type m(p, c, Cm, g, area, s);
    m.assemble_implicit(1.0, dt, v, i, mg);
    m.solve();

    vvec x;
    assign(x, m.solution());
    vvec expected = {4, 5, 6, 7, 8, 9, 10};

    EXPECT_TRUE(testing::seq_almost_eq<double>(expected, x));

    // Set dt of 2nd (middle) submatrix to zero. Solution
    // should then return voltage values for that submatrix.

    dt[1] = 0;
    v[3] = -20;
    v[4] = -30;
    m.assemble_implicit(1.0, dt, v, i, mg);
    m.solve();

    assign(x, m.solution());
    expected = {4, 5, 6, -20, -30, 9, 10};

    EXPECT_TRUE(testing::seq_almost_eq<double>(expected, x));

    // Multiplying dt by 2 and providing a dt_coeff of 0.5 should
    // give us the same result.

    for (auto& t: dt) { t *= 2; }
    m.assemble_implicit(0.5, dt, v, i, mg);
    m.solve();

    vvec x2;
    assign(x2, m.solution());
    EXPECT_EQ(x, x2);
}

TEST(matrix, step_explicit) {
    using util::assign;
    using array = matrix_type::array;

    // First test case:
    // One cell, four CVs, second CV has two children.
    // Coefficients chosen to be +/- 1.

    ivec p = {0, 0, 1, 1};
    ivec c = {0, 4};
    ivec s = {0};

    vvec g = {0, 1, 1, 1}; // [µS]
    vvec C = {1, 1, 1, 1}; // [pF]
    vvec area = {1, 1, 1, 1}; // [µm²]

    matrix_type m(p, c, C, g, area, s);

    array J = {1e3, 2e3, 5e3, 6e3}; // [A/m²]
    array v(4, 0); // [mV]
    array dt(4, 1e-3); // [ms]

    m.step_explicit(1.0, dt, v, J);
    vvec x;
    assign(x, m.solution());

    // Expect x = - dt / C * area * J,
    // as initial v is zero.

    vvec expected(4, 0);
    for (auto i: util::count_along(expected)) {
        expected[i] = -dt[i]/C[i]*J[i]*area[i];
    }
    EXPECT_TRUE(testing::seq_almost_eq<double>(expected, x));

    // Laplacian (axial conductance) matrix is
    //   [  1 -1  0  0 ]
    //   [ -1  3 -1 -1 ]
    //   [  0 -1  1  0 ]
    //   [  0 -1  0  1 ]

    v = {1., 2., 3., 4. };
    array Av = v; // [nA]
    Av[0] = v[0] - v[1];
    Av[1] = 3*v[1] - v[0] - v[2] - v[3];
    Av[2] = v[2] - v[1];
    Av[3] = v[3] - v[1];

    for (auto i: util::count_along(expected)) {
        expected[i] = v[i] - dt[i]/C[i]*(1e3*Av[i] + J[i]*area[i]);
    }

    m.step_explicit(1.0, dt, v, J);
    assign(x, m.solution());
    EXPECT_TRUE(testing::seq_almost_eq<double>(expected, x));
}

