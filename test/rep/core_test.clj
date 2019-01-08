(ns rep.core-test
  (:require
    [clojure.java.shell :refer [sh]]
    [clojure.java.io :as io]
    [midje.sweet :refer :all]
    [nrepl.server]
    [rep.core])
  (:import
    (java.io ByteArrayOutputStream OutputStreamWriter)))

(defn- rep-args [args server]
  (->> args
    (remove map?)
    (map (fn [arg]
           (case arg
             :<host+port> (str "localhost:" (:port server))
             :<port>      (str (:port server))
             arg)))))

(defn- rep-fast-driver
  "A driver which does not expect the native image to be built."
  [& args]
  (let [server (binding [*file* nil]
                 (nrepl.server/start-server))
        starting-dir (System/getProperty "user.dir")
        {:keys [port-file]
         :or {port-file ".nrepl-port"}}
        (first (filter map? args))]
    (try
      (System/setProperty "user.dir" (str starting-dir "/target"))
      (spit (str starting-dir "/target/" port-file) (str (:port server)))
      (let [out (ByteArrayOutputStream.)
            err (ByteArrayOutputStream.)]
        (let [exit-code (binding [*out* (OutputStreamWriter. out)
                                  *err* (OutputStreamWriter. err)]
                          (apply rep.core/rep (rep-args args server)))]
          {:out (.toString out)
           :err (.toString err)
           :exit exit-code}))
      (finally
        (System/setProperty "user.dir" starting-dir)
        (nrepl.server/stop-server server)))))

(defn- rep-native-driver
  "An integration driver which runs the `rep` binary."
  [& args]
  (let [server (binding [*file* nil]
                 (nrepl.server/start-server))
        starting-dir (System/getProperty "user.dir")
        {:keys [port-file]
         :or {port-file ".nrepl-port"}}
        (first (filter map? args))]
    (try
      (spit (str starting-dir "/target/" port-file) (str (:port server)))
      (apply sh "default+uberjar/rep" (concat (rep-args args server) [:dir (io/file (str starting-dir "/target"))]))
      (finally
        (nrepl.server/stop-server server)))))

(def ^:dynamic rep
  (if (= "native" (System/getenv "REP_TEST_DRIVER"))
    rep-native-driver
    rep-fast-driver))

(defn- prints [s & flags]
  (let [flags (into #{} flags)
        k (if (flags :to-stderr)
            :err
            :out)]
    (contains {k s})))

(defn- exits-with [code]
  (contains {:exit code}))

(facts "about basic evaluation of code"
  (rep "(+ 2 2)")                                  => (prints "4\n")
  (rep "(+ 1 1)")                                  => (exits-with 0)
  (rep "(println 'hello)")                         => (prints "hello\nnil\n")
  (rep "(.write ^java.io.Writer *err* \"error\")") => (prints "error" :to-stderr)
  (rep "(throw (ex-info \"x\" {}))")               => (prints #"ExceptionInfo" :to-stderr)
  (rep "(throw (ex-info \"x\" {}))")               => (exits-with 1))

(facts "about help"
  (rep "-h")     => (prints #"rep: Single-shot nREPL client")
  (rep "--help") => (prints #"rep: Single-shot nREPL client"))

(facts "about invalid switches"
  (rep "-/") => (prints #"Unknown option" :to-stderr)
  (rep "-/") => (exits-with 2))

(facts "about specifying the nREPL port"
  (let [absolute-path (str "@" (System/getProperty "user.dir") "/target/.nrepl-port")]
    (rep "-p" "@.nrepl-port" "42")                    => (prints "42\n")
    (rep "-p" "@foo.txt" "69" {:port-file "foo.txt"}) => (prints "69\n")
    (rep "-p" absolute-path  "11")                    => (prints "11\n")
    (rep "-p" :<port> "77" {:port-file "bad"})        => (prints "77\n")
    (rep "-p" :<host+port> "99" {:port-file "bad"}))  => (prints "99\n"))

(facts "about specifying the eval namespace"
  (facts "about sending a bare namespace name"
    (rep "-n" "user" "(str *ns*)")     => (prints "\"user\"\n")
    (rep "-n" "rep.core" "(str *ns*)") => (prints "\"rep.core\"\n")))

(facts "about specifying line numbers"
  (rep "(throw (Exception.))")                             => (prints #"REPL:1" :to-stderr)
  (rep "-l" "27" "(throw (Exception.))")                   => (prints #"REPL:27" :to-stderr)
  (rep "-l" "27:11"         "(do (def foo) (meta #'foo))") => (prints #":line 27")
  (rep "-l" "27:11"         "(do (def foo) (meta #'foo))") => (prints #":column 15")
  (rep "-l" "foo.clj:18"    "(do (def foo) (meta #'foo))") => (prints #":file \"foo.clj\"")
  (rep "-l" "foo.clj:18"    "(do (def foo) (meta #'foo))") => (prints #":line 18")
  (rep "-l" "foo.clj"       "(do (def foo) (meta #'foo))") => (prints #":file \"foo.clj\"")
  (rep "-l" "foo.clj:18:11" "(do (def foo) (meta #'foo))") => (prints #":file \"foo.clj\"")
  (rep "-l" "foo.clj:18:11" "(do (def foo) (meta #'foo))") => (prints #":line 18")
  (rep "-l" "foo.clj:18:11" "(do (def foo) (meta #'foo))") => (prints #":column 15"))
