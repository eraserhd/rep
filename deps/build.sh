#!/bin/bash
#
# This is designed so that CircleCI can cache these dependencies and we won't
# rebuild them.
#

graal_version="1.0.0-rc10"
clojure_commit_hash="982d1123"
spec_alpha_commit_hash="2329bb2"

set -xeo pipefail

install_graal() {
    platform="$1"
    if ! [ -d "deps/graalvm-$platform" ]; then
        curl -Lo deps/graalvm-$platform.tar.gz https://github.com/oracle/graal/releases/download/vm-$graal_version/graalvm-ce-$graal_version-$platform-amd64.tar.gz
        cd deps
        tar xzf graalvm-$platform.tar.gz
        mv graalvm-ce-* graalvm-$platform/
        cd ..
    fi
}

# Use Graal
install_graal macos
install_graal linux

if ! [ -x deps/maven/bin/mvn ]; then
    cd deps
    rm -rf maven/
    curl -Lo maven.tar.gz http://mirror.cc.columbia.edu/pub/software/apache/maven/maven-3/3.6.0/binaries/apache-maven-3.6.0-bin.tar.gz
    tar xzf maven.tar.gz
    mv apache-maven-* maven/
    cd ..
fi

source deps/path.sh

# Build a patched Clojure (see CLJ-1472)
clojure_version=$(sed -n 's/^.*org\.clojure\/clojure "\(.*\)".*/\1/p' project.clj)
if ! [ -f ~/.m2/repository/org/clojure/clojure/$clojure_version/clojure-$clojure_version.jar ]; then
    cd deps
    rm -rf clojure/
    git clone git@github.com:clojure/clojure.git
    cd clojure
    git checkout "$clojure_commit_hash"
    curl https://dev.clojure.org/jira/secure/attachment/18767/clj-1472-3.patch |patch -p1
    mvn -e install
    cd ../..
fi

# We need a patched spec.alpha, also
spec_alpha_version=$(sed -n 's/^.*org\.clojure\/spec\.alpha "\(.*\)".*/\1/p' project.clj)
if ! [ -f ~/.m2/repository/org/clojure/spec.alpha/$spec_alpha_version/spec.alpha-$spec_alpha_version.jar ]; then
    cd deps
    rm -rf spec.alpha/
    git clone git@github.com:clojure/spec.alpha.git
    cd spec.alpha
    git checkout "$spec_alpha_commit_hash"
    patch -p1 <<EOF
diff --git a/pom.xml b/pom.xml
index b22c46a..d19a888 100644
--- a/pom.xml
+++ b/pom.xml
@@ -36,7 +36,7 @@
   </developers>
 
   <properties>
-    <clojure.version>1.9.0</clojure.version>
+    <clojure.version>1.10.0-master-SNAPSHOT</clojure.version>
   </properties>
 
   <dependencies>
EOF
    mvn -e install
    cd ../..
fi
