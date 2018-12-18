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

(defn- until-status
  "Process messages until we see one with a particular status."
  [status]
  (take-until #(contains? (into #{} (:status %)) status)))

(defn- effecting
  "Build an effectful transucer which operates on the `k` value in messages."
  [k effect-fn]
  (fn [rf]
    (fn
      ([] (rf))
      ([result] (rf result))
      ([result input]
       (when (contains? input k)
         (effect-fn (get input k)))
       (rf result input)))))

(defn- print-err
  [^String s]
  (let [^java.io.Writer w *err*]
    (.write w s)
    (.flush w)))

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
        (until-status "done")
        (effecting :out print)
        (effecting :err print-err)
        (effecting :value println))
      null-reducer
      msg-seq)
    (let [^java.io.Closeable cc conn]
      (.close cc))
    0))

(defn -main
  [& args]
  (System/exit (apply rep args)))
