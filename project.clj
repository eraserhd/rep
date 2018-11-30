(defproject rep "0.1.0-SNAPSHOT"
  :description "Single-shot REPL (hence no 'L')"
  :url "http://example.com/FIXME"
  :license {:name "Eclipse Public License"
            :url "http://www.eclipse.org/legal/epl-v10.html"}
  :dependencies [[org.clojure/clojure "1.9.0"]
                 [nrepl/nrepl "0.4.0"]]
  :main ^:skip-aot rep.core
  :target-path "target/%s"
  :plugins [[io.taylorwood/lein-native-image "0.3.0"]]
  :profiles {:uberjar {:aot :all}})
