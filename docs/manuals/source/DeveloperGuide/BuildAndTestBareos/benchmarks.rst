.. _BareosBenchmarksChapter:

Benchmarks
~~~~~~~~~~

Benchmarks are introduced first for developers to test certain parts of the code when modifying performance critical code.
This is done with the help of the Google Benchmark tool available at https://github.com/google/benchmark

Benchmarking requires the installation of the Google Benchmark package (``dnf install google-benchmark`` on Fedora 33).


Adding a benchmark
^^^^^^^^^^^^^^^^^^

The new benchmark has to be listed in the CMakeLists.txt file in the benchmarks folder.

Benchmarks can be included into the CTest tests by adding the name of the benchmark to the enabled benchmarks list.


Running available benchmarks
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Build the bareos project, and binaries for the benchmarks will be created in the build directory under ``build/core/src/benchmarks/`` which you can execute and see how your bareos performs for the chosen benchmark.

If you enabled the benchmarks with CTest, you can run them all with ``ctest -R bench: --verbose`` or ``ctest -R bench:<name_of_benchmark> --verbose`` if you want to run a specific benchmark.
Adding the ``--verbose`` is important in order to see the time results.

Results can be written to a file with the ``--benchmark_out=<filename>`` option. Specify the output format with ``--benchmark_out_format={json|console|csv}``

Example:

.. code-block:: shell-session
   :caption: Outputting the result to a json file

   user@host:~$ cd ~/bareosprojects/build-bareos/core/src/benchmarks/
   user@host:~/bareosprojects/build-bareos/core/src/benchmarks$ ./restore_browser_stress_test --benchmark_out=restore_benchmark.json --benchmark_out_format=json
   2021-06-14T13:31:03+01:00
   Running ./restore_browser_stress_test
   Run on (4 X 2594 MHz CPU s)
   CPU Caches:
     L1 Data 32 KiB (x4)
     L1 Instruction 32 KiB (x4)
     L2 Unified 256 KiB (x4)
     L3 Unified 6144 KiB (x4)
   Load Average: 0.88, 0.58, 0.53
   ----------------------------------------------------------
   Benchmark                Time             CPU   Iterations
   ----------------------------------------------------------
   BM_populatetree       26.2 s          26.0 s             1
   BM_markfiles          4.03 s          4.00 s             1
   user@host:~/bareosprojects/build-bareos/core/src/benchmarks$ cat restore_benchmark.json
   {
     "context": {
       "date": "2021-06-14T13:31:03+01:00",
       "host_name": "host",
       "executable": "./restore_browser_stress_test",
       "num_cpus": 4,
       "mhz_per_cpu": 2594,
       "cpu_scaling_enabled": false,
       "caches": [
         {
           "type": "Data",
           "level": 1,
           "size": 32768,
           "num_sharing": 1
         },
         {
           "type": "Instruction",
           "level": 1,
           "size": 32768,
           "num_sharing": 1
         },
         {
           "type": "Unified",
           "level": 2,
           "size": 262144,
           "num_sharing": 1
         },
         {
           "type": "Unified",
           "level": 3,
           "size": 6291456,
           "num_sharing": 1
         }
       ],
       "load_avg": [0.88,0.58,0.53],
       "library_build_type": "release"
     },
     "benchmarks": [
       {
         "name": "BM_populatetree",
         "run_name": "BM_populatetree",
         "run_type": "iteration",
         "repetitions": 0,
         "repetition_index": 0,
         "threads": 1,
         "iterations": 1,
         "real_time": 2.6208405059000143e+01,
         "cpu_time": 2.6008753418000001e+01,
         "time_unit": "s"
       },
       {
         "name": "BM_markfiles",
         "run_name": "BM_markfiles",
         "run_type": "iteration",
         "repetitions": 0,
         "repetition_index": 0,
         "threads": 1,
         "iterations": 1,
         "real_time": 4.0332517730003019e+00,
         "cpu_time": 4.0030990340000017e+00,
         "time_unit": "s"
       }
     ]
   }



Comparing benchmarks
^^^^^^^^^^^^^^^^^^^^

Google Benchmark offers a comparison script in order two compare different benchmarks.
To use this tool, you need to clone the Google Benchmark repository, the ``compare.py`` script is located in the ``benchmark/tools`` folder.

The following shell session shows a basic usage of the tool (example is for marking 10 Million files in the restore browser):

.. code-block:: shell-session
   :caption: Comparing the same benchmark of two different builds

   user@host:~$ cd ~/benchmark/tools
   user@host:~/benchmark/tools$ ./compare.py benchmarks ~/bareosprojects/build-bareos/core/src/benchmarks/restore_browser_stress_test ~/bareosprojects/build-newcode/core/src/benchmarks/restore_browser_stress_test
   RUNNING: /home/user/bareosprojects/build-bareos/core/src/benchmarks/restore_browser_stress_test --benchmark_out=/tmp/tmpl3v_lavr
   2021-06-14T12:24:22+01:00
   Running /home/user/bareosprojects/build-bareos/core/src/benchmarks/restore_browser_stress_test
   Run on (4 X 2594 MHz CPU s)
   CPU Caches:
     L1 Data 32 KiB (x4)
     L1 Instruction 32 KiB (x4)
     L2 Unified 256 KiB (x4)
     L3 Unified 6144 KiB (x4)
   Load Average: 2.53, 2.74, 1.39
   ----------------------------------------------------------
   Benchmark                Time             CPU   Iterations
   ----------------------------------------------------------
   BM_populatetree       25.0 s          24.8 s             1
   BM_markfiles          4.03 s          3.99 s             1
   RUNNING: /home/user/bareosprojects/build-newcode/core/src/benchmarks/restore_browser_stress_test --benchmark_out=/tmp/tmpcfzev4gt
   2021-06-14T12:24:54+01:00
   Running /home/user/bareosprojects/build-newcode/core/src/benchmarks/restore_browser_stress_test
   Run on (4 X 2594 MHz CPU s)
   CPU Caches:
     L1 Data 32 KiB (x4)
     L1 Instruction 32 KiB (x4)
     L2 Unified 256 KiB (x4)
     L3 Unified 6144 KiB (x4)
   Load Average: 2.16, 2.63, 1.40
   ----------------------------------------------------------
   Benchmark                Time             CPU   Iterations
   ----------------------------------------------------------
   BM_populatetree       28.1 s          27.9 s             1
   BM_markfiles          4.15 s          4.11 s             1
   Comparing /home/user/bareosprojects/build-bareos/core/src/benchmarks/restore_browser_stress_test to /home/user/bareosprojects/build-newcode/core/src/benchmarks/restore_browser_stress_test
   Benchmark                         Time             CPU      Time Old      Time New       CPU Old       CPU New
   --------------------------------------------------------------------------------------------------------------
   BM_populatetree                +0.1234         +0.1257            25            28            25            28
   BM_markfiles                   +0.0297         +0.0291             4             4             4             4

What it does is for every benchmark from the first run it looks for the benchmark with exactly the same name in the second run, and then compares the results. If the names differ, the benchmark is omitted from the diff.
As you can note, the values in Time and CPU columns are calculated as (new - old) / old.

The same could be done by comparing the json outputs of both benchmarks, or even by comparing a binary with a json output.

Example:

.. code-block:: shell-session
   :caption: Comparing benchmarks with json outputs

   user@host:~/benchmark/tools$ ./compare.py benchmarks ~/bareosprojects/build-bareos/core/src/benchmarks/restore_benchmark.json ~/bareosprojects/build-newcode/core/src/benchmarks/restore_benchmark.json
   Comparing /home/user/bareosprojects/build-bareos/core/src/benchmarks/restore_benchmark.json to /home/user/bareosprojects/build-bareos/core/src/benchmarks/restore_benchmark.json
   Benchmark                         Time             CPU      Time Old      Time New       CPU Old       CPU New
   --------------------------------------------------------------------------------------------------------------
   BM_populatetree                -0.2390         -0.2390            26            20            26            20
   BM_markfiles                   -0.0338         -0.0322             3             3             3             3
