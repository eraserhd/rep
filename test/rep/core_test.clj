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

(facts "about basic evaluation of code"
  (rep "(+ 2 2)") => (contains {:stdout "4\n"})
  (rep "(+ 1 1)") => (contains {:exit-code 0})
  (rep "(println 'hello)") => (contains {:stdout "hello\nnil\n"})
  (rep "(.write ^java.io.Writer *err* \"error\")") => (contains {:stderr "error"}))

