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
  (rep "(throw (ex-info \"x\" {}))")               => (exits-with 1)
  (rep "-n" "ns.does.not.exist" "(+ 1 1)")         => (exits-with 1))

(facts "about help"
  (rep "-h")     => (prints #"rep: Single-shot nREPL client")
  (rep "--help") => (prints #"rep: Single-shot nREPL client"))

(facts "about invalid switches"
  (rep "-/") => (prints #"invalid option" :to-stderr)
  (rep "-/") => (exits-with 2))

(facts "about specifying the nREPL port"
  (rep "-p" "@.nrepl-port" "42")                         => (prints "42\n")
  (rep "-p" "@.does-not-exist" "42")                     => (prints #"\.does-not-exist: No such file or directory\n" :to-stderr)
  (rep "-p" "@foo.txt" "69" {:port-file "foo.txt"})      => (prints "69\n")
  (rep "-p" "${port}" "77" {:port-file "bad"})           => (prints "77\n")
  (rep "-p" "localhost:${port}" "99" {:port-file "bad"}) => (prints "99\n")
  (rep "-p" "@.nrepl-port@target/src/foo/bar.clj" "111") => (prints "111\n")
  (rep "-p" "@${user.dir}/target/.nrepl-port" "11")      => (prints "11\n")
  (rep "-p" "@.nrepl-port@/not-exist/foo/bar/baz" "91")  => (prints "rep: No ancestor of /not-exist/foo/bar/baz contains .nrepl-port\n" :to-stderr))

(facts "about specifying the eval namespace"
  (facts "about sending a bare namespace name"
    (rep "-n" "user" "(str *ns*)")     => (prints "\"user\"\n")
    (rep "-n" "rep.core-test" "(str *ns*)") => (prints "\"rep.core-test\"\n")))

(facts "about specifying line numbers"
  (rep "(throw (Exception.))")                             => (prints #"\(:1\)" :to-stderr)
  (rep "-l" "27" "(throw (Exception.))")                   => (prints #"\(:27\)" :to-stderr)
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

(facts "about --print"
  (fact "it can print arbitrary keys"
    (rep "--print=ns" "(+ 1 1)") => (prints "user"))
  (fact "it overrides any default formats the first time given"
    (rep "--print=value,1,-%{value}-%n" "(+ 2 3)") => (prints "-5-\n"))
  (fact "it can print to stderr"
    (rep "--print=value,2,>%{value}<" "(+ 1 1)") => (prints ">2<" :to-stderr))
  (fact "it can be given multiple times for one KEY"
    (rep "--print=value,1,<%{value}>" "--print=value,1,<<%{value}>>" "2") => (prints #"<2><<2>>"))
  (fact "it can print integral values"
    (rep "--print=intvalue,1,<%{intvalue}>" "--op=rep-test-op") => (prints #"<67>")))

(facts "about --no-print"
  (fact "it can suppress a key"
    (rep "--no-print=out" "(println 'whaat?)") => (prints "nil\n")))

(facts "about sending additional fields"
  (rep "--op=rep-test-op" "--send=foo,string,quux") => (prints "foo=\"quux\";\"hello\"\n")
  (rep "--op=rep-test-op" "--send=bar,integer,42")  => (prints "bar=42;\"hello\"\n"))

(facts "about the examples in the documentation"
  (rep "--op=ls-sessions" "--print=sessions,1,%{sessions,session=%.%n;}") => (prints #"^session=[a-fA-F0-9]{6}"))
