{
  description = "A single-shot nREPL client designed for shell invocation.";
  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, flake-utils }:
    (flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        rep = pkgs.callPackage ./derivation.nix {};
      in {
        packages = {
          default = rep;
          inherit rep;
        };
      }))
    // (flake-utils.lib.eachSystem ["x86_64-darwin" "x86_64-linux"] (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        rep = pkgs.callPackage ./derivation.nix {};
      in {
        checks = {
          unit-test = let
            java-deps = import ./test-deps.nix { inherit (pkgs) fetchMavenArtifact fetchgit lib; };
          in pkgs.stdenv.mkDerivation {
            name = "rep-unit-tests";
            src = ./.;
            buildInputs = with pkgs; [ rep jdk ];
            CLASSPATH = java-deps.makeClasspaths { extraClasspaths = [ "test" "src" ]; };
            REP_TEST_DRIVER = "native";
            REP_TO_TEST = "${rep}/bin/rep";
            buildPhase = ''
              rm -rf target/
              mkdir -p target/
              java clojure.main -e "
                (require 'midje.repl)
                (System/exit (min 255 (:failures (midje.repl/load-facts :all))))
              "
            '';
            installPhase = ''
              touch $out
            '';
          };
        };
      }))
    // {
      overlays.default = final: prev: {
        rep = prev.callPackage ./derivation.nix {};
      };
    };
}
