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

(defn- contains-status? [message status]
  (contains? (into #{} (:status message)) status))

(defn- take-until
  "Transducer like take-while, except keeps the last value tested."
  [pred]
  (fn [rf]
    (fn
      ([] (rf))
      ([result] (rf result))
      ([result input]
       (let [result (rf result input)]
         (cond
          (reduced? result) result
          (pred input)      (reduced result)
          :else             result))))))

(defn- print-values
  "Effectful transducer which prints REPL output."
  [rf]
  (fn
    ([] (rf))
    ([result] (rf result))
    ([result input]
     (when-some [value (:value input)]
       (println value))
     (rf result input))))

(defn- null-reducer
  "A reducing function which does nothing."
  ([] nil)
  ([result] result)
  ([result input] result))

(defn rep
  [& args]
  (let [conn (nrepl/connect :port (nrepl-port))
        client (nrepl/client conn 60000)
        session (nrepl/client-session client)
        msg-seq (session {:op "eval" :code (apply str args)})]
    (transduce
      (comp
        (take-until #(contains-status? % "done"))
        print-values)
      null-reducer
      msg-seq)
    (let [^java.io.Closeable cc conn]
      (.close cc))
    0))

(defn -main
  [& args]
  (System/exit (apply rep args)))
