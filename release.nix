{ nixpkgs ? (import ./nixpkgs.nix), ... }:
let
  pkgs = import nixpkgs { config = {}; };
  rep = pkgs.callPackage ./derivation.nix {};
in {
  unit-test = let
    java-deps = import ./test-deps.nix { inherit pkgs; };
  in pkgs.stdenv.mkDerivation {
    name = "rep-unit-tests";
    src = ./.;
    buildInputs = with pkgs; [ rep jdk ] ++ map (x: x.path) java-deps.packages;
    buildPhase = ''
      for jar in deps/*; do
        CLASSPATH="$jar:$CLASSPATH"
      done
      CLASSPATH=test:src:$CLASSPATH
      export CLASSPATH
      REP_TEST_DRIVER=native \
        REP_TO_TEST='${rep}/bin/rep' \
        java clojure.main -e "
          (require 'midje.repl)
          (System/exit (min 255 (:failures (midje.repl/load-facts :all))))
        "
    '';
    installPhase = ''
      touch $out
    '';
  };
}
