(ns rep.format-test
  (:refer-clojure :exclude [format])
  (:require
    [midje.sweet :refer :all]
    [rep.format :refer [format]]))

(facts "about formatting"
  (fact "text without % characters is passed through"
    (format "hello" {}) => "hello")
  (fact "%% is a literal %"
    (format "h%%l" {}) => "h%l")
  (fact "%n is a newline"
    (format "h%nl" {}) => "h\nl")
  (fact "%{name} interpolates a key value"
    (format "h%{foo}x" {:foo "bar"}) => "hbarx"))
