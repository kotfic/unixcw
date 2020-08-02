clang-tidy -checks="*,-llvm-header-guard,-readability-braces-around-statements,-readability-else-after-return,-clang-diagnostic-deprecated-declarations" *.c *.h -- -I.. -I../cwutils/
