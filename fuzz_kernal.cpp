// libFuzzer harness for H.I.L.B.E.R.T.'s current equality-certificate kernel.
// Build with Clang and -DHILBERT_NO_MAIN; the implementation is included so
// this harness exercises the exact same parser, evaluator, proof search and kernel.
#define HILBERT_NO_MAIN
#include "HILBERT.cpp"

#include <cstdlib>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    using namespace hilbert;
    if (size == 0 || size > 1024) return 0;
    std::string input;
    input.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        const unsigned char c = data[i];
        if (c == 0 || c > 126) return 0;
        input.push_back(static_cast<char>(c));
    }
    try {
        Statement s = Parser(input).statement();
        auto candidate = Euclid().prove(s);
        if (!candidate) return 0;
        std::string why;
        if (!Kernel().verify(s, *candidate, why)) return 0;

        // A verified result must survive exact evaluation at all sampled points
        // satisfying the statement's assumptions. Undefined points are skipped.
        std::set<std::string> names;
        vars(s.left, names); vars(s.right, names);
        for (const auto &a : s.assumptions) { vars(a.left, names); vars(a.right, names); }
        for (int sample = 0; sample < 24; ++sample) {
            std::map<std::string, Rational> env;
            int x = sample - 12;
            for (const auto &name : names) {
                env[name] = Rational(x);
                x = (x * 7 + 3) % 19;
            }
            try {
                if (assumptionsHold(s, env) &&
                    !relationHolds(s.rel, eval(s.left, env), eval(s.right, env))) {
                    std::abort();
                }
            } catch (const Error &) { /* Undefined sample is not a refutation. */ }
        }

        // Mutation smoke tests: a certificate with a modified final key or a
        // deleted step must never be accepted as the original proof.
        Proof mutated = *candidate;
        mutated.finalKey += "#mutated";
        if (Kernel().verify(s, mutated, why)) std::abort();
        if (!candidate->steps.empty()) {
            mutated = *candidate;
            mutated.steps.pop_back();
            if (Kernel().verify(s, mutated, why)) std::abort();
        }
    } catch (const Error &) {
        // Most arbitrary byte strings are not valid mathematical statements.
    } catch (const std::exception &) {
        // Malformed fuzz inputs must not escape the harness as uncaught errors.
    }
    return 0;
}
