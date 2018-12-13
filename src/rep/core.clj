(ns rep.core
  (:require
   [nrepl.core :as nrepl])
  (:gen-class))

(defn- nrepl-port
  []
  (Long/parseLong (slurp ".nrepl-port")))

(defn- help
  []
  (println "rep: single-shot nREPL client")
  (println "Syntax:")
  (println "  rep [OPTIONS] CODE ...")
  (println)
  (println "Options:")
  (println)
  (System/exit 0))

(defn -main
  [& args]
  (help))
