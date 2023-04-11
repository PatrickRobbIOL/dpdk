#!/bin/sh -xe

DEF_LIB=shared

# Builds are run as root in containers, no need for sudo
[ "$(id -u)" != '0' ] || alias sudo=



# configure_coredump() {
#     # No point in configuring coredump without gdb
#     which gdb >/dev/null || return 0
#     ulimit -c unlimited
#     sudo sysctl -w kernel.core_pattern=/tmp/dpdk-core.%e.%p
# }

# catch_coredump() {
#     ls /tmp/dpdk-core.*.* 2>/dev/null || return 0
#     for core in /tmp/dpdk-core.*.*; do
#         binary=$(sudo readelf -n $core |grep $(pwd)/build/ 2>/dev/null |head -n1)
#         [ -x $binary ] || binary=
#         sudo gdb $binary -c $core \
#             -ex 'info threads' \
#             -ex 'thread apply all bt full' \
#             -ex 'quit'
#     done |tee -a build/gdb.log
#     return 1
# }

OPTS="$OPTS -Ddefault_library=$DEF_LIB"

OPTS="$OPTS -Dwerror=true"

if [ -d build ]; then
    meson configure build $OPTS
else
    meson setup build $OPTS
fi
meson configure build
ninja -C build

