(defproject rep "0.1.0-SNAPSHOT"
  :description "Single-shot REPL (hence no 'L')"
  :url "http://example.com/FIXME"
  :license {:name "Eclipse Public License"
            :url "http://www.eclipse.org/legal/epl-v10.html"}
  :dependencies [[org.clojure/clojure "1.10.0-master-SNAPSHOT"]
                 [org.clojure/spec.alpha "0.2.177-SNAPSHOT"]
                 [nrepl/nrepl "0.5.1"]]
  :main ^:skip-aot rep.core
  :global-vars {*warn-on-reflection* true}
  :native-image {:name "rep"
                 :opts ["--verbose"
                        "--report-unsupported-elements-at-runtime"
                        "-H:ReflectionConfigurationFiles=reflectconfig.json"]}
  :target-path "target/%s"
  :plugins [[io.taylorwood/lein-native-image "0.3.0"]]
  :profiles {:uberjar {:aot :all
                       :native-image {:opts ["-Dclojure.compiler.direct-linking=true"]}}})
                                      
