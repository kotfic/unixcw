if [ "$#" -ne 1 ]; then
    echo "wrong args"
    exit
fi

path_to_check=$1

echo "path_to_check = "$path_to_check

root=`pwd`

echo "root = " $root



clang-tidy-11 $path_to_check -- -I$HOME/include -I$root/src/cwutils/ -I$root/src/ -DXCWCP_WITH_REC_TEST


