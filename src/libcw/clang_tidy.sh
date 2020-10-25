CHECK_INCLUDE_PATHS="-I. -I.. -I../cwutils/"

CHECK_CHECKS="*"
CHECK_CHECKS+=",-llvm-header-guard"
CHECK_CHECKS+=",-readability-braces-around-statements,-hicpp-braces-around-statements"
CHECK_CHECKS+=",-readability-else-after-return,-clang-diagnostic-deprecated-declarations"
CHECK_CHECKS+=",-readability-magic-numbers,-cppcoreguidelines-avoid-magic-numbers"
CHECK_CHECKS+=",-readability-redundant-control-flow"
CHECK_CHECKS+=",-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling"

#clang-tidy    -checks=$CHECK_CHECKS *.c *.h -- $CHECK_INCLUDE_PATHS
clang-tidy-10 -checks=$CHECK_CHECKS *.c *.h -- $CHECK_INCLUDE_PATHS
