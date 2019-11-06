{ stdenv, pkgs, fetchurl, graalvm8, leiningen, maven, jdk, ... }:

let
  java-deps = import ./deps.nix { inherit pkgs; };
in stdenv.mkDerivation {
  pname = "rep";
  version = "0.1.2";

  src = ./.;

  buildInputs = [
    graalvm8
    leiningen
    maven
  ] ++ map (x: x.path) java-deps.packages;

  buildPhase = ''
    mkdir -p home
    export HOME=$(pwd)/home
    lein native-image
  '';

  meta = with stdenv.lib; {
    description = "TODO: fill me in";
    homepage = https://github.com/eraserhd/rep;
    license = licenses.unlicense;
    platforms = platforms.all;
    maintainers = [ maintainers.eraserhd ];
  };
}
