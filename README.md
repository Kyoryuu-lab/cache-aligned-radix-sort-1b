<h1>cache-aligned-radix-sort-1b</h1>
<p>Hardware-tuned RadixSort sorting 1 Billion 32-bit elements in ~1 second via L1/L2 cache alignment</p>

<h2>Project description</h2>
<p>This project maximizes memory bandwidth and cache residency on multi-core systems (Apple Silicon M-series Unified Memory / high-throughput CPUs):</p>
<ol>
  <li>Processes <code>N = 1,000,000,000</code> pseudorandom 32-bit integers (3.72 GiB per vector buffer).</li>
  <li>
    <dl>
      <dt>Hardware and Cache Architecture Optimizations:</dt>
      <dd><b>L1/L2 Cache Sizing:</b> Histogram tables (256 × 8 bytes = 2 KB per thread) reside fully in ultra-fast L1 cache.</dd>
      <dd><b>Ping-Pong Buffering:</b> Eliminates dynamic reallocations by toggling two pre-allocated continuous memory pointers (<code>src</code> and <code>dst</code>).</dd>
      <dd><b>TLB-Safe Scatter:</b> Restricts radix width to <code>r = 8</code> bit (4 passes) to stay well within CPU Translation Lookaside Buffer limits.</dd>
    </dl>
  </li>
  <li>Achieves a throughput exceeding <b>930 million elements per second</b>.</li>
</ol>

<h2>Project files</h2>
<table>
  <tr>
    <th colspan="2">cache-aligned-radix-sort-1b</th>
  </tr>
  <tr>
    <td>CMakeLists.txt</td>
    <td>High-optimization CMake script (<code>-O3 -march=native</code>, C++20)</td>
  </tr>
  <tr>
    <td>main.cpp</td>
    <td>Cache-aligned RadixSort implementation with phase timing instrumentation</td>
  </tr>
  <tr>
    <td>README.md</td>
    <td>Detailed architectural discussion and memory bandwidth analysis</td>
  </tr>
</table>

<h2>Performance Benchmark (N = 1,000,000,000)</h2>
<table>
  <tr>
    <th>Algorithm Variant</th>
    <th>Radix (r)</th>
    <th>Total Execution Time</th>
    <th>Speedup vs std::qsort</th>
    <th>Status</th>
  </tr>
  <tr>
    <td><b>std::qsort (Baseline)</b></td>
    <td>—</td>
    <td>130,147.00 ms (~130 s)</td>
    <td>1.00x</td>
    <td>Correct</td>
  </tr>
  <tr>
    <td><b>Variante 2 (r = 4 Bit)</b></td>
    <td>4 Bit (8 passes)</td>
    <td>4,528.34 ms</td>
    <td>28.74x</td>
    <td>Correct</td>
  </tr>
  <tr>
    <td><b>Variante 2 [OPTIMAL]</b></td>
    <td><b>8 Bit (4 passes)</b></td>
    <td><b>1,065.85 ms (~1.06 s)</b></td>
    <td><b>122.11x</b></td>
    <td><b>Correct</b></td>
  </tr>
  <tr>
    <td><b>Variante 2 (r = 16 Bit)</b></td>
    <td>16 Bit (2 passes)</td>
    <td>1,885.01 ms</td>
    <td>69.04x</td>
    <td>Correct</td>
  </tr>
</table>

<h2>Installation and launch</h2>
<ol>
  <b>
    <li>
      <dl>
        <dt>Clone repository</dt>
        <dd>
          <code>git clone https://github.com/Kyoryuu-lab/cache-aligned-radix-sort-1b.git</code>
        </dd>
        <dd>
          <code>cd cache-aligned-radix-sort-1b</code>
        </dd>
      </dl>
    </li>
  </b>
  <b>
    <li>
      <dl>
        <dt>Build in Release mode</dt>
        <dd>
          <code>cmake -B build -DCMAKE_BUILD_TYPE=Release</code>
        </dd>
        <dd>
          <code>cmake --build build --config Release</code>
        </dd>
      </dl>
    </li>
  </b>
  <b>
    <li>
      <dl>
        <dt>Execute 1-billion element test</dt>
        <dd>
          <code>./build/main</code>
        </dd>
      </dl>
    </li>
  </b>
</ol>

<h2>Example of console output</h2>
<samp>
  <p>
    ========================================================================<br />
    PVA Rechneruebung 2: Profiling & Memory-Safe Benchmark<br />
    N = 1000000000 (3.72 GB per Buffer) | Threads: 15<br />
    ========================================================================<br />
    [*] Starte std::qsort (Sequentiell)... Bitte warten!<br />
    std::qsort abgeschlossen: 130147.00 ms<br />
    <br />
    [*] Benchmark: Variante 2 (r = 8 Bit) [OPT]...<br />
    Total Time:    1065.85 ms | Speedup: 122.11x | Effizienz: 814.04 %<br />
      - Phase 1 (Hist):     467.25 ms (43.84 %)<br />
      - Phase 2 (Scan):       0.04 ms ( 0.00 %)<br />
      - Phase 3 (Scatter):  598.53 ms (56.15 %)<br />
    Status: SORTIERT (OK)<br />
    ========================================================================
  </p>
</samp>
