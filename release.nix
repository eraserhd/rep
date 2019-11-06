{ nixpkgs ? (import ./nixpkgs.nix), ... }:
let
  pkgs = import nixpkgs { config = {}; };
  rep = pkgs.callPackage ./derivation.nix {};
in {
  unit-test = pkgs.stdenv.mkDerivation {
    name = "rep-unit-tests";
    src = ./.;
    buildInputs = with pkgs; [ rep jdk ];
    buildPhase = ''
      for jar in deps/*; do
        CLASSPATH="$jar:$CLASSPATH"
      done
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
