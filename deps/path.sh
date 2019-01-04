if [ `uname` = Darwin ]; then
    PATH="$PWD/deps/graalvm-macos/Contents/Home/bin":"$PWD/deps/maven/bin":$PATH
else
    PATH="$PWD/deps/graalvm-linux/bin":"$PWD/deps/maven/bin":$PATH
fi
export PATH
