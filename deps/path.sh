if [ `uname` = Darwin ]; then
    PATH="$PWD/deps/graalvm-macos/Contents/Home/bin":$PATH
else
    PATH="$PWD/graalvm-linux/bin":$PATH
fi
export PATH
