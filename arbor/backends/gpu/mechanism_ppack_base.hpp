#pragma once

// Base class for parameter packs for GPU generated kernels:
// will be included by .cu generated sources.

#include <arbor/mechanism.hpp>
#include <arbor/fvm_types.hpp>

namespace arb {
namespace gpu {

// Parameter pack base:
struct mechanism_ppack_base {
    fvm_index_type width_;
    fvm_index_type n_detectors_;

    const fvm_index_type* vec_ci_;
    const fvm_index_type* vec_di_;
    const fvm_value_type* vec_t_;
    const fvm_value_type* vec_dt_;
    const fvm_value_type* vec_v_;
    fvm_value_type* vec_i_;
    fvm_value_type* vec_g_;
    const fvm_value_type* temperature_degC_;
    const fvm_value_type* diam_um_;
    const fvm_value_type* time_since_spike_;

    const fvm_index_type* node_index_;
    const fvm_index_type* multiplicity_;

    const fvm_value_type* weight_;
};

} // namespace gpu
} // namespace arb
