#include <benchmark/benchmark.h>

#include <pthread.h>

namespace blackhole {
namespace benchmark {
namespace {

/// Cheap operation on OS X, relatively expensive on linux.
void thread_name(::benchmark::State& state) {
    while (state.KeepRunning()) {
        char buffer[16];
        ::pthread_getname_np(::pthread_self(), buffer, 16);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(thread_name);

}  // namespace
}  // namespace benchmark
}  // namespace blackhole
