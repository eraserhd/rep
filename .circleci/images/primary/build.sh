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

# Build a patched Clojure (see CLJ-1472)
cd /tmp
git clone https://github.com/clojure/clojure.git
cd clojure
git checkout "$clojure_commit_hash"

# dead link
#curl https://dev.clojure.org/jira/secure/attachment/18767/clj-1472-3.patch |patch -p1
curl 'https://api.media.atlassian.com/file/2af127ec-44e4-4bcd-9e0d-40fa2c737c00/binary?client=44579ec4-9d32-4a41-b3c1-ceed2de443de&collection=&dl=true&token=eyJhbGciOiJIUzI1NiJ9.eyJpc3MiOiI0NDU3OWVjNC05ZDMyLTRhNDEtYjNjMS1jZWVkMmRlNDQzZGUiLCJhY2Nlc3MiOnsidXJuOmZpbGVzdG9yZTpmaWxlOjcyMjc5YjNiLTgwNDctNGE0YS04MjhhLWZkNWM2MjUwMGI0MCI6WyJyZWFkIl0sInVybjpmaWxlc3RvcmU6ZmlsZToyYWYxMjdlYy00NGU0LTRiY2QtOWUwZC00MGZhMmM3MzdjMDAiOlsicmVhZCJdLCJ1cm46ZmlsZXN0b3JlOmZpbGU6OTAwNzcyNzItYmEwNC00NTFiLTk3MTgtNTA1OTNlOGRmOGU1IjpbInJlYWQiXSwidXJuOmZpbGVzdG9yZTpmaWxlOjcxYjY4ZmRkLTg5NTMtNGQ2Ny04ZGQ1LWEzY2Y0Yjg3ZTY3YiI6WyJyZWFkIl0sInVybjpmaWxlc3RvcmU6ZmlsZTpiOGFiOWNiMy1iMDdlLTQ2MzctOWM4ZC1lODZmYWUwNjFmY2EiOlsicmVhZCJdLCJ1cm46ZmlsZXN0b3JlOmZpbGU6M2NiOWYxM2ItYTg1MC00YTI5LWI0ZjgtNTc4MWI4NDQ0ZTdkIjpbInJlYWQiXX0sImV4cCI6MTU3MjYxNTAzNiwibmJmIjoxNTcyNjE0MDc2fQ.fG1uECS3YhkSIYqGgcEZvIahpxGDcNCNN0JAdPaJINw' | patch -p1


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
