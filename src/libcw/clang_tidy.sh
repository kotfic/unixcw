CHECK_INCLUDE_PATHS="-I. -I.. -I../cwutils/"

CHECK_CHECKS="*"
CHECK_CHECKS+=",-llvm-header-guard"
CHECK_CHECKS+=",-readability-braces-around-statements,-readability-else-after-return,-clang-diagnostic-deprecated-declarations"

clang-tidy -checks=$CHECK_CHECKS *.c *.h -- $CHECK_INCLUDE_PATHS
# clang-tidy-10 -checks="*,-llvm-header-guard,-readability-braces-around-statements,-readability-else-after-return,-clang-diagnostic-deprecated-declarations,-hicpp-braces-around-statements,-readability-magic-numbers,-cppcoreguidelines-avoid-magic-numbers,-readability-redundant-control-flow" *.c *.h -- -I.. -I../cwutils/
# clang-tidy-10 -checks="*,-llvm-header-guard,-readability-braces-around-statements,-readability-else-after-return,-clang-diagnostic-deprecated-declarations,-hicpp-braces-around-statements,-readability-magic-numbers,-cppcoreguidelines-avoid-magic-numbers,-readability-redundant-control-flow,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling" *.c *.h -- -I.. -I../cwutils/
