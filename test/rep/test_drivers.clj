(ns rep.test-drivers
  (:require
    [clojure.java.shell :refer [sh]]
    [clojure.java.io :as io]
    [clojure.string :as str]
    [midje.sweet :refer :all]
    [nrepl.misc :refer [response-for]]
    [nrepl.server]
    [nrepl.transport :as t]))

(defn- rep-args [args server user-dir]
  (->> args
    (remove map?)
    (map (fn [arg]
           (-> arg
             (str/replace "${port}" (str (:port server)))
             (str/replace "${user.dir}" user-dir))))))

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
    (apply sh rep-bin (concat (rep-args args server starting-dir) [:dir (io/file (str starting-dir "/target"))]))))

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
        (t/send transport (response-for message :status :done :value value :intvalue 67)))
      (f message))))

(def ^:private handler
  (nrepl.server/default-handler wrap-rep-test-op))

(defn rep [& args]
  (let [server (binding [*file* nil]
                 (nrepl.server/start-server :handler handler))]
    (try
      (apply rep-native-driver server args)
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
