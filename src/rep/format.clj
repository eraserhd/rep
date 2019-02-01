(ns rep.format
  (:refer-clojure :exclude [format]))

(defn format
  [pattern data]
  (let [^StringBuilder sb (StringBuilder.)]
    (loop [pattern pattern]
      (if-let [p' (condp re-matches pattern
                    #"(?s)([^%]+)(.*)"      :>> (fn [[_ literal-text pattern]]
                                                  (.append sb literal-text)
                                                  pattern)
                    #"(?s)%%(.*)"           :>> (fn [[_ pattern]]
                                                  (.append sb \%)
                                                  pattern)
                    #"(?s)%n(.*)"           :>> (fn [[_ pattern]]
                                                  (.append sb \newline)
                                                  pattern)
                    #"(?s)%\{([^}]*)\}(.*)" :>> (fn [[_ var-name pattern]]
                                                  (when-let [value (get data (keyword var-name))]
                                                    (.append sb value))
                                                  pattern)
                    #""                     :>> (constantly nil))]
        (recur p')
        (.toString sb)))))
