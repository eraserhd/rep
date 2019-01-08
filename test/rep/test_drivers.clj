(ns rep.test-drivers
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

(defn rep-fast-driver
  "A driver which does not expect the native image to be built."
  [server & args]
  (let [starting-dir (System/getProperty "user.dir")
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
        (System/setProperty "user.dir" starting-dir)))))

(defn rep-native-driver
  "An integration driver which runs the `rep` binary."
  [server & args]
  (let [starting-dir (System/getProperty "user.dir")
        {:keys [port-file]
         :or {port-file ".nrepl-port"}}
        (first (filter map? args))]
    (spit (str starting-dir "/target/" port-file) (str (:port server)))
    (apply sh "default+uberjar/rep" (concat (rep-args args server) [:dir (io/file (str starting-dir "/target"))]))))

(def ^:dynamic *driver*
  (if (= "native" (System/getenv "REP_TEST_DRIVER"))
    rep-native-driver
    rep-fast-driver))

(defn rep [& args]
  (let [server (binding [*file* nil]
                 (nrepl.server/start-server))]
    (try
      (apply *driver* server args)
      (finally
        (nrepl.server/stop-server server)))))

(defn prints [s & flags]
  (let [flags (into #{} flags)
        k (if (flags :to-stderr)
            :err
            :out)]
    (contains {k s})))

(defn exits-with [code]
  (contains {:exit code}))
