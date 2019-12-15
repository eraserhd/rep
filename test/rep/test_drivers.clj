(ns rep.test-drivers
  (:require
    [clojure.java.shell :refer [sh]]
    [clojure.java.io :as io]
    [clojure.string :as str]
    [midje.sweet :refer :all]
    [nrepl.misc :refer [response-for]]
    [nrepl.server]
    [nrepl.transport :as t]
    [rep.core])
  (:import
    (java.io ByteArrayOutputStream OutputStreamWriter)))

(defn- rep-args [args server user-dir]
  (->> args
    (remove map?)
    (map (fn [arg]
           (-> arg
             (str/replace "${port}" (str (:port server)))
             (str/replace "${user.dir}" user-dir))))))

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
            err (ByteArrayOutputStream.)
            exit-code (binding [*out* (OutputStreamWriter. out)
                                *err* (OutputStreamWriter. err)]
                        (apply rep.core/rep (rep-args args server starting-dir)))]
        {:out (.toString out)
         :err (.toString err)
         :exit exit-code})
      (finally
        (System/setProperty "user.dir" starting-dir)))))

(defn rep-native-driver
  "An integration driver which runs the `rep` binary."
  [server & args]
  (let [rep-bin (or (System/getenv "REP_TO_TEST")
                    "default/rep")
        starting-dir (System/getProperty "user.dir")
        {:keys [port-file]
         :or {port-file ".nrepl-port"}}
        (first (filter map? args))]
    (spit (str starting-dir "/target/" port-file) (str (:port server)))
    (apply sh rep-bin (concat (rep-args args server) [:dir (io/file (str starting-dir "/target"))]))))

(def ^:dynamic *driver*
  (if (= "native" (System/getenv "REP_TEST_DRIVER"))
    rep-native-driver
    rep-fast-driver))

(defn- wrap-rep-test-op [f]
  (fn [{:keys [op transport] :as message}]
    (if (= "rep-test-op" op)
      (let [value (str
                    (if-let [foo (:foo message)]
                      (format "foo=%s;" (pr-str foo))
                      "")
                    (if-let [bar (:bar message)]
                      (format "bar=%s;" (pr-str bar))
                      "")
                    (pr-str "hello"))]
        (t/send transport (response-for message :status :done :value value)))
      (f message))))

(def ^:private handler
  (nrepl.server/default-handler wrap-rep-test-op))

(defn rep [& args]
  (let [server (binding [*file* nil]
                 (nrepl.server/start-server :handler handler))]
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
