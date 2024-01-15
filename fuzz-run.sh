export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
export AFL_SKIP_CPUFREQ=1
export AFL_NO_UI=1
export AFL_EXIT_ON_TIME=6000
# Baseline
# AFL_CUSTOM_MUTATOR_LIBRARY=custom_mutators/aflpp_baseline/aflpp-mutator.so timeout -s SIGTERM 48h ./afl-fuzz -i ./input_seeds -o ./output ~/libjpeg-turbo/build/djpeg

# png custom mutator
# /home/hxxzhang/libpng/contrib/oss-fuzz/libpng_read_fuzzer.cc
# AFL_CUSTOM_MUTATOR_LIBRARY=custom_mutators/aflpp/aflpp-mutator.so timeout -s SIGTERM 24h ./afl-fuzz -i ./input_seeds_png -o ./output_png ~/libpng/contrib/libtests/readpng

AFL_CUSTOM_MUTATOR_LIBRARY=custom_mutators/aflpp/aflpp-mutator.so timeout -s SIGTERM 24h ./afl-fuzz -i ./input_seeds -o ./output_topktopp3 ~/libjpeg-turbo/build/djpeg