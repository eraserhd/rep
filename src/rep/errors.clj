(ns rep.errors
  (:require
   [clojure.string :as str]))

(defprotocol HasErrorMessage
  (error-message [e]))

(extend-protocol HasErrorMessage
  Object
  (error-message [o]
    (str o))

  Exception
  (error-message [e]
    (.getMessage e))

  java.io.FileNotFoundException
  (error-message [e]
    ;; Graal's message is just the filename, OpenJDK is filename, space, and
    ;; a parenthesized error message.
    (let [filename (str/replace (.getMessage e) #" \(.*$" "")]
      (str filename ": not found."))))
