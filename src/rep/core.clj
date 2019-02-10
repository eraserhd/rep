(ns rep.core
  (:require
   [clojure.tools.cli :as cli]
   [nrepl.core :as nrepl]
   [rep.format :as format])
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

(defn- printing-key
  "Build an effectful transucer which prints the `k` value in messages."
  [{:keys [key ^long fd format]}]
  (fn [rf]
    (fn
      ([] (rf))
      ([result] (rf result))
      ([result input]
       (when (contains? input key)
         (let [^java.io.Writer out (case fd
                                     1 *out*
                                     2 *err*)
               text ^String (format/format format input)]
           (.write out text)
           (.flush out)))
       (rf result input)))))

(defn- printing
  "Build an effectful transducer which prints the keys in m."
  [print-specs]
  (->> print-specs
    (map printing-key)
    (apply comp)))

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

(defn- parse-print-argument [arg]
  (condp re-matches arg
    #"(?s)([^,]*),(\d+),(.*)" :>> (fn [[_ k fd fmt]]
                                    {:key (keyword k)
                                     :fd (Long/parseLong fd)
                                     :format fmt})
    #"([^,]*),(\d+)"          :>> (fn [[_ k fd]]
                                    {:key (keyword k)
                                     :fd (Long/parseLong fd)
                                     :format (str "%{" k "}")})
    #"([^,]*)"                :>> (fn [[_ k]]
                                    {:key (keyword k)
                                     :fd 1
                                     :format (str "%{" k "}")})))

(defn- add-print-argument [opts _ spec]
  (update opts
          :print
          (fn [specs]
            (-> (remove (fn [{:keys [key default?]}]
                          (and default? (= key (:key spec))))
                        specs)
              vec
              (conj spec)))))

(defn- parse-send-argument [arg]
  (condp re-matches arg
    #"(?s)([^,]+),(string|integer),(.*)" :>> (fn [[_ k ty v]]
                                               [(keyword k)
                                                (case ty
                                                  "string"  v
                                                  "integer" (Long/parseLong v))])))

(defn- add-send-argument [opts _ [k v]]
  (update opts :send assoc k v))

(def ^:private cli-options
  [["-h" "--help"                   "Show this help screen."]
   ["-l" "--line LINE[:COLUMN]"     "Specify code's starting LINE and COLUMN."
    :parse-fn parse-line-argument
    :default (parse-line-argument "1")
    :default-desc "1"]
   ["-n" "--namespace NS"           "Evaluate expressions in NS."
    :default "user"]
   [nil "--op OP"                   "Send OP as the nREPL operation."
    :default "eval"]
   ["-p" "--port [HOST:]PORT|@FILE" "Connect to HOST at PORT, which may be read from FILE."
    :default "@.nrepl-port"]
   [nil "--print KEY[,FD[,FORMAT]]" "Print KEY from response messages to FD using FORMAT."
    :default [{:key :out, :fd 1, :format "%{out}", :default? true}
              {:key :err, :fd 2, :format "%{err}", :default? true}
              {:key :value, :fd 1, :format "%{value}%n", :default? true}]
    :default-desc ["out,1,%{name}", "err,2,%{err}", "value,1,%{value}%n"]
    :parse-fn parse-print-argument
    :assoc-fn add-print-argument]
   [nil "--send KEY,TYPE,VALUE"     "Send KEY: VALUE in request."
    :default {}
    :default-desc ""
    :parse-fn parse-send-argument
    :assoc-fn add-send-argument]])

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
                                (:send options)
                                {:op (:op options)
                                 :ns (:namespace options)
                                 :code (apply str arguments)}))
        result (transduce
                 (comp
                   (until-status "done")
                   (printing (:print options))
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
