#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <tbb/tbb.h>

// Структура для сбора замеров по фазам
struct PhaseTimings {
    double hist_ms = 0.0;
    double scan_ms = 0.0;
    double scatter_ms = 0.0;
    double total_ms = 0.0;
};

// Заполнение существующего буфера in-place (без новых аллокаций памяти)
void regenerate_data_inplace(std::vector<uint32_t>& data, uint32_t seed = 1337) {
    std::mt19937 rng(seed);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = rng();
    }
}

int compare_uint(const void* a, const void* b) {
    uint32_t arg1 = *static_cast<const uint32_t*>(a);
    uint32_t arg2 = *static_cast<const uint32_t*>(b);
    return (arg1 > arg2) - (arg1 < arg2);
}

// Профилированная Variante 2 с переиспользованием внешнего буфера temp
template <int RADIX_BITS>
PhaseTimings radix_sort_v2_profiled(std::vector<uint32_t>& data, std::vector<uint32_t>& temp) {
    constexpr int NUM_BUCKETS = 1 << RADIX_BITS;
    constexpr uint32_t RADIX_MASK = NUM_BUCKETS - 1;

    const size_t n = data.size();
    uint32_t* src = data.data();
    uint32_t* dst = temp.data();

    const size_t p = tbb::info::default_concurrency();
    const size_t block_size = (n + p - 1) / p;

    std::vector<size_t> hist(p * NUM_BUCKETS);
    std::vector<size_t> offsets(p * NUM_BUCKETS);

    PhaseTimings timings;
    auto t_start_total = std::chrono::high_resolution_clock::now();

    for (int shift = 0; shift < 32; shift += RADIX_BITS) {
        std::fill(hist.begin(), hist.end(), 0);

        // --- Фаза 1: Histogram ---
        auto t1 = std::chrono::high_resolution_clock::now();
        tbb::parallel_for(size_t(0), p, [&](size_t t) {
            size_t start = t * block_size;
            size_t end = std::min(start + block_size, n);
            size_t* my_hist = &hist[t * NUM_BUCKETS];
            for (size_t i = start; i < end; ++i) {
                my_hist[(src[i] >> shift) & RADIX_MASK]++;
            }
        });
        auto t2 = std::chrono::high_resolution_clock::now();
        timings.hist_ms += std::chrono::duration<double, std::milli>(t2 - t1).count();

        // --- Фаза 2: Prefix-Scan (Column-Major) ---
        auto t3 = std::chrono::high_resolution_clock::now();
        size_t running_sum = 0;
        for (size_t b = 0; b < NUM_BUCKETS; ++b) {
            for (size_t t = 0; t < p; ++t) {
                offsets[t * NUM_BUCKETS + b] = running_sum;
                running_sum += hist[t * NUM_BUCKETS + b];
            }
        }
        auto t4 = std::chrono::high_resolution_clock::now();
        timings.scan_ms += std::chrono::duration<double, std::milli>(t4 - t3).count();

        // --- Фаза 3: Scatter ---
        auto t5 = std::chrono::high_resolution_clock::now();
        tbb::parallel_for(size_t(0), p, [&](size_t t) {
            size_t start = t * block_size;
            size_t end = std::min(start + block_size, n);
            size_t* my_offsets = &offsets[t * NUM_BUCKETS];
            for (size_t i = start; i < end; ++i) {
                uint32_t val = src[i];
                dst[my_offsets[(val >> shift) & RADIX_MASK]++] = val;
            }
        });
        auto t6 = std::chrono::high_resolution_clock::now();
        timings.scatter_ms += std::chrono::duration<double, std::milli>(t6 - t5).count();

        std::swap(src, dst);
    }

    if (src != data.data()) {
        std::copy(temp.begin(), temp.end(), data.begin());
    }

    auto t_end_total = std::chrono::high_resolution_clock::now();
    timings.total_ms = std::chrono::duration<double, std::milli>(t_end_total - t_start_total).count();

    return timings;
}

void print_phase_breakdown(const std::string& name, const PhaseTimings& t, double t_seq, unsigned int p) {
    double sp = t_seq / t.total_ms;
    double eff = (sp / p) * 100.0;

    std::cout << "\n>>> " << name << ":\n";
    std::cout << "  - Total Time:   " << std::setw(8) << std::fixed << std::setprecision(2) << t.total_ms << " ms"
              << " | Speedup: " << std::setw(5) << sp << "x | Effizienz: " << eff << " %\n";
    std::cout << "  - Phase 1 (Hist):    " << std::setw(8) << t.hist_ms << " ms ("
              << std::setw(5) << (t.hist_ms / t.total_ms) * 100.0 << " %)\n";
    std::cout << "  - Phase 2 (Scan):    " << std::setw(8) << t.scan_ms << " ms ("
              << std::setw(5) << (t.scan_ms / t.total_ms) * 100.0 << " %)\n";
    std::cout << "  - Phase 3 (Scatter): " << std::setw(8) << t.scatter_ms << " ms ("
              << std::setw(5) << (t.scatter_ms / t.total_ms) * 100.0 << " %)\n";
}

int main() {
    const size_t N = 1'000'000'000;
    const unsigned int p = tbb::info::default_concurrency();

    std::cout << "========================================================================\n";
    std::cout << "PVA Rechneruebung 2: Profiling & Memory-Safe Benchmark\n";
    std::cout << "N = " << N << " (" << (N * sizeof(uint32_t)) / (1024 * 1024 * 1024) << " GB per Buffer) | Threads: " << p << "\n";
    std::cout << "========================================================================\n";

    // Выделяем строго 2 вектора (4 GB + 4 GB = 8 GB RAM суммарно на всю программу)
    std::vector<uint32_t> data(N);
    std::vector<uint32_t> temp(N);

    // 1. Baseline: std::qsort
    std::cout << "[*] Generiere Daten fuer std::qsort...\n";
    regenerate_data_inplace(data);

    std::cout << "[*] Starte std::qsort (Sequentiell)... Bitte warten!\n";
    auto t1 = std::chrono::high_resolution_clock::now();
    std::qsort(data.data(), data.size(), sizeof(uint32_t), compare_uint);
    auto t2 = std::chrono::high_resolution_clock::now();
    double t_seq = std::chrono::duration<double, std::milli>(t2 - t1).count();

    std::cout << "std::qsort abgeschlossen: " << t_seq << " ms\n";
    std::cout << "------------------------------------------------------------------------\n";

    // 2. Тестирование r = 4 Bit (8 проходов)
    std::cout << "[*] Benchmark: Variante 2 (r = 4 Bit)...";
    regenerate_data_inplace(data);
    auto t_4b = radix_sort_v2_profiled<4>(data, temp);
    print_phase_breakdown("Variante 2 (r = 4 Bit, 8 Durchlaeufe)", t_4b, t_seq, p);

    // 3. Тестирование r = 8 Bit (4 прохода) — Оптимум
    std::cout << "\n[*] Benchmark: Variante 2 (r = 8 Bit)...";
    regenerate_data_inplace(data);
    auto t_8b = radix_sort_v2_profiled<8>(data, temp);
    print_phase_breakdown("Variante 2 (r = 8 Bit, 4 Durchlaeufe) [OPT]", t_8b, t_seq, p);
    std::cout << "    Status: " << (std::is_sorted(data.begin(), data.end()) ? "SORTIERT (OK)" : "FEHLER") << "\n";

    // 4. Тестирование r = 16 Bit (2 прохода)
    std::cout << "\n[*] Benchmark: Variante 2 (r = 16 Bit)...";
    regenerate_data_inplace(data);
    auto t_16b = radix_sort_v2_profiled<16>(data, temp);
    print_phase_breakdown("Variante 2 (r = 16 Bit, 2 Durchlaeufe)", t_16b, t_seq, p);

    std::cout << "\n========================================================================\n";
    return 0;
}
