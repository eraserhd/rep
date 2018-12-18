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
        starting-dir (System/getProperty "user.dir")]
    (try
      (System/setProperty "user.dir" (str starting-dir "/target"))
      (spit (str starting-dir "/target/.nrepl-port") (str (:port server)))
      (let [out (ByteArrayOutputStream.)
            err (ByteArrayOutputStream.)]
        (let [exit-code (binding [*out* (OutputStreamWriter. out)
                                  *err* (OutputStreamWriter. err)]
                          (apply rep.core/rep args))]
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
  (rep "(throw (ex-info \"x\" {}))")               => (prints #"ExceptionInfo" :to-stderr))
