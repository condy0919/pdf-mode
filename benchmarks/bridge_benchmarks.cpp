#include <benchmark/benchmark.h>

#include "bridge.hpp"

emacs_env* env;
int plugin_is_GPL_compatible;

int emacs_module_init(struct emacs_runtime* runtime) EMACS_NOEXCEPT {
    // leak the emacs_env pointer
    env = runtime->get_environment(runtime);

    benchmark::RunSpecifiedBenchmarks();
    return 0;
}

static void BM_type(benchmark::State& state) {
    yapdf::emacs::Env e(env);
    using yapdf::emacs::Value;

    auto val = e.make<Value::Type::String>("Hello").value();
    for (auto _ : state) {
        const bool result = (val.type() == 4);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_type);

static void BM_typeof(benchmark::State& state) {
    yapdf::emacs::Env e(env);
    using yapdf::emacs::Value;

    auto val = e.make<Value::Type::String>("Hello").value();
    for (auto _ : state) {
        const bool result = (val.typeOf() == e.intern("string").value());
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_typeof);
