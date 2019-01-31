(ns rep.core-test
  (:require
    [midje.sweet :refer :all]
    [rep.test-drivers :refer [rep prints exits-with]]))

(facts "about basic evaluation of code"
  (rep "(+ 2 2)")                                  => (prints "4\n")
  (rep "(+ 1 1)")                                  => (exits-with 0)
  (rep "(println 'hello)")                         => (prints "hello\nnil\n")
  (rep "(.write ^java.io.Writer *err* \"error\")") => (prints "error" :to-stderr)
  (rep "(throw (ex-info \"x\" {}))")               => (prints #"ExceptionInfo" :to-stderr)
  (rep "(throw (ex-info \"x\" {}))")               => (exits-with 1))

(facts "about help"
  (rep "-h")     => (prints #"rep: Single-shot nREPL client")
  (rep "--help") => (prints #"rep: Single-shot nREPL client"))

(facts "about invalid switches"
  (rep "-/") => (prints #"Unknown option" :to-stderr)
  (rep "-/") => (exits-with 2))

(facts "about specifying the nREPL port"
  (let [absolute-path (str "@" (System/getProperty "user.dir") "/target/.nrepl-port")]
    (rep "-p" "@.nrepl-port" "42")                    => (prints "42\n")
    (rep "-p" "@foo.txt" "69" {:port-file "foo.txt"}) => (prints "69\n")
    (rep "-p" absolute-path  "11")                    => (prints "11\n")
    (rep "-p" :<port> "77" {:port-file "bad"})        => (prints "77\n")
    (rep "-p" :<host+port> "99" {:port-file "bad"}))  => (prints "99\n"))

(facts "about specifying the eval namespace"
  (facts "about sending a bare namespace name"
    (rep "-n" "user" "(str *ns*)")     => (prints "\"user\"\n")
    (rep "-n" "rep.core" "(str *ns*)") => (prints "\"rep.core\"\n")))

(facts "about specifying line numbers"
  (rep "(throw (Exception.))")                             => (prints #"REPL:1" :to-stderr)
  (rep "-l" "27" "(throw (Exception.))")                   => (prints #"REPL:27" :to-stderr)
  (rep "-l" "27:11"         "(do (def foo) (meta #'foo))") => (prints #":line 27")
  (rep "-l" "27:11"         "(do (def foo) (meta #'foo))") => (prints #":column 15")
  (rep "-l" "foo.clj:18"    "(do (def foo) (meta #'foo))") => (prints #":file \"foo.clj\"")
  (rep "-l" "foo.clj:18"    "(do (def foo) (meta #'foo))") => (prints #":line 18")
  (rep "-l" "foo.clj"       "(do (def foo) (meta #'foo))") => (prints #":file \"foo.clj\"")
  (rep "-l" "foo.clj:18:11" "(do (def foo) (meta #'foo))") => (prints #":file \"foo.clj\"")
  (rep "-l" "foo.clj:18:11" "(do (def foo) (meta #'foo))") => (prints #":line 18")
  (rep "-l" "foo.clj:18:11" "(do (def foo) (meta #'foo))") => (prints #":column 15"))

(facts "about specifying the operation"
  (rep "--op=eval" "(+ 1 2)") => (prints "3\n")
  (rep "--op=rep-test-op") => (prints "\"hello\"\n"))

(facts "about printing responses"
  (rep "--print=foo-key" "(+ 1 1)") => (prints "2\n")
  (rep "--print=value,1,%s\n" "(+ 2 3)") => (prints "5\n")
  (rep "--print=value,2,>%s<" "(+ 1 1)") => (prints ">2<" :to-stderr)
  (rep "--no-print=value" "(+ 1 1)") =not=> (prints "2\n")
  (rep "--op=ls-sessions" "--print=sessions") => (prints #"^\[\""))
