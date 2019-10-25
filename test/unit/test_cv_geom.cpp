#include <arbor/util/optional.hpp>
#include <arbor/cable_cell.hpp>
#include <arbor/morph/morphology.hpp>
#include <arbor/morph/locset.hpp>

#include "morph/em_morphology.hpp"
#include "fvm_layout.hpp"

#include "common.hpp"
#include "../common_cells.hpp"

using namespace arb;

// TODO: factor out test morphologies from this and test_cv_policy.

namespace {
    std::vector<msample> make_samples(unsigned n) {
        std::vector<msample> ms;
        for (auto i: make_span(n)) ms.push_back({{0., 0., (double)i, 0.5}, 5});
        return ms;
    }

    // Test morphologies for CV determination:
    // Samples points have radius 0.5, giving an initial branch length of 1.0
    // for morphologies with spherical roots.

    const morphology m_empty;

    // spherical root, one branch
    const morphology m_sph_b1{sample_tree(make_samples(1), {mnpos}), true};

    // regular root, one branch
    const morphology m_reg_b1{sample_tree(make_samples(2), {mnpos, 0u}), false};

    // spherical root, six branches
    const morphology m_sph_b6{sample_tree(make_samples(8), {mnpos, 0u, 1u, 0u, 3u, 4u, 4u, 4u}), true};

    // regular root, six branches
    const morphology m_reg_b6{sample_tree(make_samples(7), {mnpos, 0u, 1u, 1u, 2u, 2u, 2u}), false};

    // regular root, six branches, mutiple top level branches.
    const morphology m_mlt_b6{sample_tree(make_samples(7), {mnpos, 0u, 1u, 1u, 0u, 4u, 4u}), false};
}

TEST(cv_layout, empty) {
    cable_cell empty_cell = make_cable_cell(m_empty);
    cv_geometry geom = cv_geometry_from_ends(empty_cell, ls::nil());

    EXPECT_TRUE(geom.cv_ends.empty());
    EXPECT_TRUE(geom.cv_ends_divs.empty());
    EXPECT_TRUE(geom.cv_cables.empty());
    EXPECT_TRUE(geom.cv_cables_divs.empty());
}

TEST(cv_layout, trivial) {
    for (auto& morph: {m_sph_b1, m_reg_b1, m_sph_b6, m_mlt_b6}) {
        cable_cell cell = make_cable_cell(morph);

        // Four equivalent ways of specifying one CV comprising whole cell:
        cv_geometry geom1 = cv_geometry_from_ends(empty_cell, ls::nul());
        cv_geometry geom2 = cv_geometry_from_ends(empty_cell, ls::root());
        cv_geometry geom3 = cv_geometry_from_ends(empty_cell, ls::terminal());
        cv_geometry geom4 = cv_geometry_from_ends(empty_cell, join(ls::root(), ls::terminal());

        EXPECT_EQ(geom1.cv_cables(), geom2.cv_cables());
        EXPECT_EQ(geom1.cv_cables(), geom3.cv_cables());
        EXPECT_EQ(geom1.cv_cables(), geom4.cv_cables());

        EXPECT_EQ(1u, geom1.size());

        mlocation_list root_and_terminals = join(ls::root(), ls::terminal()).thingify(cell.morphology());
        EXPECT_EQ(root_and_terminals, geom1.end_points(0));

        mcable_list all_cables = reg::all().thingify(cell.morphology());
        EXPECT_EQ(all_cables, geom1.cables(0));
    }
}
