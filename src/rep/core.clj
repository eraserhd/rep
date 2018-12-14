(ns rep.core
  (:require
   [nrepl.core :as nrepl])
  (:gen-class))

(defn- nrepl-port
  []
  (let [dir (System/getProperty "user.dir")]
    (Long/parseLong (slurp (str dir "/.nrepl-port")))))

(defn- help
  []
  (println "rep: single-shot nREPL client")
  (println "Syntax:")
  (println "  rep [OPTIONS] CODE ...")
  (println)
  (println "Options:")
  (println)
  (System/exit 0))

(defn rep
  [& args]
  (let [conn (nrepl/connect :port (nrepl-port))
        client (nrepl/client conn 60000)
        session (nrepl/client-session client)
        msg-seq (session {:op "eval" :code (apply str args)})]
    (loop [[{:keys [status value out err]} & msgs] msg-seq]
      (when out
        (print out))
      (when err
        (binding [*out* *err*]
          (print err)))
      (when value
        (println value))
      (when-not (contains? (into #{} status) "done")
        (recur msgs)))
    (let [^java.io.Closeable cc conn]
      (.close cc))))

(defn -main
  [& args]
  (System/exit (apply rep args)))
