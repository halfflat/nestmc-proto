#pragma once

#include <iosfwd>
#include <unordered_map>
#include <utility>
#include <vector>

#include <arbor/fvm_types.hpp>

#include "fvm_layout.hpp"

#include "backends/gpu/gpu_store_types.hpp"
#include "backends/gpu/stimulus.hpp"

namespace arb {
namespace gpu {

/*
 * Ion state fields correspond to NMODL ion variables, where X
 * is replaced with the name of the ion. E.g. for calcium 'ca':
 *
 *     Field   NMODL variable   Meaning
 *     -------------------------------------------------------
 *     iX_     ica              calcium ion current density
 *     eX_     eca              calcium ion channel reversal potential
 *     Xi_     cai              internal calcium concentration
 *     Xo_     cao              external calcium concentration
 */

struct ion_state {
    iarray node_index_; // Instance to CV map.
    array iX_;          // (A/m²) current density
    array eX_;          // (mV) reversal potential
    array Xi_;          // (mM) internal concentration
    array Xo_;          // (mM) external concentration

    array init_Xi_;     // (mM) area-weighted initial internal concentration
    array init_Xo_;     // (mM) area-weighted initial external concentration
    array reset_Xi_;    // (mM) area-weighted user-set internal concentration
    array reset_Xo_;    // (mM) area-weighted user-set internal concentration
    array init_eX_;     // (mM) initial reversal potential

    array charge;       // charge of ionic species (global, length 1)

    ion_state() = default;

    ion_state(
        int charge,
        const fvm_ion_config& ion_data,
        unsigned align
    );

    // Set ion concentrations to weighted proportion of default concentrations.
    void init_concentration();

    // Set ionic current density to zero.
    void zero_current();

    // Zero currents, reset concentrations, and reset reversal potential from
    // initial values.
    void reset();
};

struct istim_state {
    // Immutable data (post construction/initialization):
    iarray accu_index_;     // Instance to accumulator index (accu_stim_ index) map.
    iarray accu_to_cv_;     // Accumulator index to CV map.

    array frequency_;       // (Hz) stimulus frequency per instance.
    array envl_amplitudes_; // (A/m²) stimulus envelope amplitudes, partitioned by instance.
    array envl_times_;      // (A/m²) stimulus envelope timepoints, partitioned by instance.
    iarray envl_divs_;      // Partition divisions for envl_ arrays,

    // Mutable data:
    array accu_stim_;       // (A/m²) accumulated stim current / CV area, one per CV with a stimulus.
    iarray envl_index_;     // Per instance index into envl_ arrays, corresponding to last sample time.

    // Parameter pack presents pointers to state arrays, relevant shared state to GPU kernels.
    // Initialized at state construction.
    istim_pp ppack_;

    // Zero stim current.
    void zero_current();

    // Zero stim current, reset indices.
    void reset();

    // Contribute to current density:
    void add_current();

    // Number of stimuli:
    std::size_t size() const;

    // Construct state from i_clamp data; references to shared state vectors are used to initialize ppack.
    istim_state(const fvm_stimulus_config& stim_data, const array& time, const iarray& cv_to_intdom, array& current_density);

    istim_state() = default;
};

struct shared_state {
    fvm_size_type n_intdom = 0;   // Number of distinct integration domains.
    fvm_size_type n_detector = 0; // Max number of detectors on all cells.
    fvm_size_type n_cv = 0;       // Total number of CVs.
    fvm_size_type n_gj = 0;       // Total number of GJs.

    iarray cv_to_intdom;     // Maps CV index to intdom index.
    iarray cv_to_cell;       // Maps CV index to cell index.
    gjarray gap_junctions;   // Stores gap_junction info.
    array time;              // Maps intdom index to integration start time [ms].
    array time_to;           // Maps intdom index to integration stop time [ms].
    array dt_intdom;         // Maps intdom index to (stop time) - (start time) [ms].
    array dt_cv;             // Maps CV index to dt [ms].
    array voltage;           // Maps CV index to membrane voltage [mV].
    array current_density;   // Maps CV index to current density [A/m²].
    array conductivity;      // Maps CV index to membrane conductivity [kS/m²].

    array init_voltage;      // Maps CV index to initial membrane voltage [mV].
    array temperature_degC;  // Maps CV to local temperature (read only) [°C].
    array diam_um;           // Maps CV to local diameter (read only) [µm].

    array time_since_spike;   // Stores time since last spike on any detector, organized by cell.
    iarray src_to_spike;      // Maps spike source index to spike index

    istim_state stim_data;
    std::unordered_map<std::string, ion_state> ion_data;
    deliverable_event_stream deliverable_events;

    shared_state() = default;

    shared_state(
        fvm_size_type n_intdom,
        fvm_size_type n_cell,
        fvm_size_type n_detector,
        const std::vector<fvm_index_type>& cv_to_intdom_vec,
        const std::vector<fvm_index_type>& cv_to_cell_vec,
        const std::vector<fvm_gap_junction>& gj_vec,
        const std::vector<fvm_value_type>& init_membrane_potential,
        const std::vector<fvm_value_type>& temperature_K,
        const std::vector<fvm_value_type>& diam,
        const std::vector<fvm_index_type>& src_to_spike,
        unsigned align
    );

    void add_ion(
        const std::string& ion_name,
        int charge,
        const fvm_ion_config& ion_data);

    void configure_stimulus(const fvm_stimulus_config&);

    void zero_currents();

    void ions_init_concentration();

    // Set time_to to earliest of time+dt_step and tmax.
    void update_time_to(fvm_value_type dt_step, fvm_value_type tmax);

    // Set the per-intdom and per-compartment dt from time_to - time.
    void set_dt();

    // Update gap_junction state
    void add_gj_current();

    // Update stimulus state and add current contributions.
    void add_stimulus_current();

    // Return minimum and maximum time value [ms] across cells.
    std::pair<fvm_value_type, fvm_value_type> time_bounds() const;

    // Return minimum and maximum voltage value [mV] across cells.
    // (Used for solution bounds checking.)
    std::pair<fvm_value_type, fvm_value_type> voltage_bounds() const;

    // Take samples according to marked events in a sample_event_stream.
    void take_samples(
        const sample_event_stream::state& s,
        array& sample_time,
        array& sample_value);

    void reset();
};

// For debugging only
std::ostream& operator<<(std::ostream& o, shared_state& s);

} // namespace gpu
} // namespace arb
