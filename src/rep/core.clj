(ns rep.core
  (:require
   [nrepl.core :as nrepl])
  (:gen-class))

(defn- nrepl-port
  []
  (Long/parseLong (slurp ".nrepl-port")))

(defn -main
  [& args]
  (println "port: " (nrepl-port))
  (println "Hello, world!"))
