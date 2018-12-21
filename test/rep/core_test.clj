(ns rep.core-test
  (:require
    [midje.sweet :refer :all]
    [nrepl.server]
    [rep.core])
  (:import
    (java.io ByteArrayOutputStream OutputStreamWriter)))

(defn- rep
  [& args]
  (let [server (nrepl.server/start-server)
        starting-dir (System/getProperty "user.dir")
        rep-args (->> args
                      (remove map?)
                      (map (fn [arg]
                             (case arg
                               :<host+port> (str "localhost:" (:port server))
                               :<port>      (str (:port server))
                               arg))))
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
                          (apply rep.core/rep rep-args))]
          {:stdout (.toString out)
           :stderr (.toString err)
           :exit-code exit-code}))
      (finally
        (System/setProperty "user.dir" starting-dir)
        (nrepl.server/stop-server server)))))

(defn- prints [s & flags]
  (let [flags (into #{} flags)
        k (if (flags :to-stderr)
            :stderr
            :stdout)]
    (contains {k s})))

(defn- exits-with [code]
  (contains {:exit-code code}))

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
    (rep "-n" "rep.core" "(str *ns*)") => (prints "\"rep.core\"\n"))
  (facts "about sending an ns form"
    (rep "-n" "\n (ns rep.core) more.." "(str *ns*)") => (prints "\"rep.core\"\n")))
