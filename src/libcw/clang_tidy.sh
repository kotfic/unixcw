clang-tidy -checks="*,-llvm-header-guard,-readability-braces-around-statements,-readability-else-after-return" *.c *.h -- -I.. -I../cwutils/
