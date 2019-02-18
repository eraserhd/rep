(defproject rep "0.1.3-SNAPSHOT"
  :description "Single-shot REPL (hence no 'L')"
  :url "https://github.com/eraserhd/rep"
  :license {:name "Eclipse Public License"
            :url "http://www.eclipse.org/legal/epl-v10.html"}
  :min-lein-version "2.8.3"
  :dependencies [[org.clojure/clojure "1.10.0-master-SNAPSHOT"]
                 [org.clojure/spec.alpha "0.2.177-SNAPSHOT"]
                 [org.clojure/tools.cli "0.4.1"]
                 [nrepl/nrepl "0.5.3"]]
  :main ^:skip-aot rep.core
  :global-vars {*warn-on-reflection* true}
  :native-image {:name "rep"
                 :opts ["--verbose"
                        "--no-server"
                        "-J-Xmx3g"
                        "--report-unsupported-elements-at-runtime"
                        "-H:ReflectionConfigurationFiles=reflectconfig.json"]}
  :target-path "target/%s"
  :plugins [[io.taylorwood/lein-native-image "0.3.0"]
            [lein-midje "3.2.1"]]
  :profiles {:dev {:dependencies [[midje "1.9.4"]]}
             :uberjar {:aot :all
                       :native-image {:opts ["-Dclojure.compiler.direct-linking=true"]}}})
