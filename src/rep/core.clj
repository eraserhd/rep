(ns rep.core
  (:require
   [clojure.tools.cli :refer [parse-opts]]
   [nrepl.core :as nrepl])
  (:gen-class))

(def ^:private cli-options
  [["-h" "--help" "Show this help message."]])

(defn- nrepl-port
  []
  (Long/parseLong (slurp ".nrepl-port")))

(defn -main
  [& args]
  (let [{:keys [errors options arguments summary]} (parse-opts args cli-options)]
    (cond
     (seq errors)
     (binding [*out* *err*]
       (doseq [error errors]
         (println error))
       (System/exit 1))

     (:help options)
     (do
       (println "rep: single-shot nREPL client")
       (println "Syntax:")
       (println "  rep [OPTIONS] CODE ...")
       (println)
       (println "Options:")
       (println summary)
       (println)
       (System/exit 0))

     :else
     (do
       (println "port: " (nrepl-port))
       (println "Hello, world!")))))
