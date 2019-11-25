#!/bin/bash
#
# This is designed so that CircleCI can cache these dependencies and we won't
# rebuild them.
#

clojure_commit_hash="982d1123"
clojure_version="1.10.0-master-SNAPSHOT"
spec_alpha_commit_hash="2329bb2"
spec_alpha_version="0.2.177-SNAPSHOT"

set -xeo pipefail

rep_home=$PWD

# Build a patched Clojure (see CLJ-1472)
cd /tmp
git clone https://github.com/clojure/clojure.git
cd clojure
git checkout "$clojure_commit_hash"

# [CLJ-1472] The locking macro fails bytecode verification on Graal native-image and ART runtime
# https://clojure.atlassian.net/browse/CLJ-1472
patch -p1 < $rep_home/CLJ-1472-3.patch

mvn -Dmaven.test.skip install

# We need a patched spec.alpha, also
cd /tmp
git clone https://github.com/clojure/spec.alpha.git
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
mvn -Dmaven.test.skip install
