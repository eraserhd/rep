(ns rep.format-test
  (:refer-clojure :exclude [format])
  (:require
    [clojure.test :refer [deftest is]]
    [rep.format :refer [format]]))

(deftest t-format
  (is (= "hello" (format "hello" {}))              "text without % characters is passed through")
  (is (= (format "h%%l" {}) "h%l"))                "%% is a literal %"
  (is (= (format "h%nl" {}) "h\nl"))               "%n is a newline"
  (is (= (format "h%{foo}x" {:foo "bar"}) "hbarx") "%{name} interpolates a key value"))
