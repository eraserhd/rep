(ns rep.core
  (:require
   [clojure.tools.cli :as cli]
   [nrepl.core :as nrepl])
  (:import
   (java.io File))
  (:gen-class))

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

(defn- report-exceptions
  [rf]
  (fn
    ([] (rf))
    ([result] (rf result))
    ([result input]
     (cond-> result
       (contains? input :ex)
       (assoc :exit-code 1)))))

(defn- null-reducer
  "A reducing function which does nothing."
  ([] nil)
  ([result] result)
  ([result input] result))

(defn- parse-line-argument [arg]
  (condp re-matches arg
    #"(.*):(\d+):(\d+)" :>> (fn [[_ file line column]]
                             {:file file
                              :line (Long/parseLong line)
                              :column (Long/parseLong column)})
    #"(\d+):(\d+)"      :>> (fn [[_ line column]]
                             {:line (Long/parseLong line)
                              :column (Long/parseLong column)})
    #"(.*):(\d+)"       :>> (fn [[_ file line]]
                             {:file file
                              :line (Long/parseLong line)})
    #"(\d+)"            :>> (fn [[_ line]]
                             {:line (Long/parseLong line)})
    #"(.*)"             :>> (fn [[_ file]]
                              {:file file
                               :line 1})))

(def ^:private cli-options
  [["-l" "--line LINE[:COLUMN]"     "Specify code's starting LINE and COLUMN."
    :parse-fn parse-line-argument
    :default (parse-line-argument "1")
    :default-desc "1"]
   ["-n" "--namespace NS"           "Evaluate expressions in NS."
    :default "user"]
   ["-p" "--port [HOST:]PORT|@FILE" "Connect to HOST at PORT, which may be read from FILE."
    :default "@.nrepl-port"]
   ["-h" "--help"                   "Show this help screen."]])

(defmulti command
  (fn [opts]
    (cond
      (:errors opts)          :syntax
      (:help (:options opts)) :help
      :else                   :eval)))

(defmethod command :syntax
  [{:keys [errors]}]
  (doseq [^String e errors]
    (.write ^java.io.Writer *err* (str e \newline)))
  (.flush ^java.io.Writer *err*)
  2)

(defmethod command :help
  [{:keys [summary]}]
  (println "rep: Single-shot nREPL client")
  (println "Syntax:")
  (println "  rep [OPTIONS] CODE ...")
  (println)
  (println "Options:")
  (println summary)
  (println)
  0)

(defn- nrepl-connect-args
  [opts]
  (let [^String dir (System/getProperty "user.dir")]
    (loop [option-value (:port (:options opts))]
      (if-some [[_ ^String filename :as x] (re-matches #"^@(.*)" option-value)]
        (if (.isAbsolute (File. filename))
          (recur (slurp filename))
          (recur (slurp (str (File. dir filename)))))
        (if-some [[_ host port] (re-matches #"(.*):(.*)" option-value)]
          [:host host :port (Long/parseLong port)]
          [:port (Long/parseLong option-value)])))))

(defmethod command :eval
  [{:keys [options arguments] :as opts}]
  (let [conn (apply nrepl/connect (nrepl-connect-args opts))
        client (nrepl/client conn 60000)
        session (nrepl/client-session client)
        msg-seq (session (merge (:line options)
                                {:op "eval"
                                 :ns (:namespace options)
                                 :code (apply str arguments)}))
        result (transduce
                 (comp
                   (until-status "done")
                   (effecting :out print)
                   (effecting :err print-err)
                   (effecting :value println)
                   report-exceptions)
                 null-reducer
                 {:exit-code 0}
                 msg-seq)
        ^java.io.Closeable cc conn]
    (.close cc)
    (:exit-code result)))

(defn rep
  [& args]
  (let [opts (cli/parse-opts args cli-options)]
    (command opts)))

(defn -main
  [& args]
  (System/exit (apply rep args)))
