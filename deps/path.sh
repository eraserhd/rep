if [ `uname` = Darwin ]; then
    PATH="$PWD/deps/graalvm-macos/Contents/Home/bin":"$PWD/maven/bin":$PATH
else
    PATH="$PWD/graalvm-linux/bin":"$PWD/maven/bin":$PATH
fi
export PATH
