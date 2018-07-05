#include <vector>

#include <arbor/common_types.hpp>
#include <arbor/spike.hpp>

#include "thread_private_spike_store.hpp"

namespace arb {

struct local_spike_store_type {
    threading::enumerable_thread_specific<std::vector<spike>> buffers_;
};

thread_private_spike_store::thread_private_spike_store():
    impl_(new local_spike_store_type)
{}

thread_private_spike_store::~thread_private_spike_store() {}

std::vector<spike> thread_private_spike_store::gather() const {
    std::vector<spike> spikes;
    unsigned num_spikes = 0u;
    for (auto& b: impl_->buffers_) {
        num_spikes += b.size();
    }
    spikes.reserve(num_spikes);

    for (auto& b: impl_->buffers_) {
        spikes.insert(spikes.begin(), b.begin(), b.end());
    }

    return spikes;
}

std::vector<spike>& thread_private_spike_store::get() {
    return impl_->buffers_.local();
}

void thread_private_spike_store::clear() {
    for (auto& b: impl_->buffers_) {
        b.clear();
    }
}

} // namespace arb