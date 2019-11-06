{ stdenv, pkgs, clojure, fetchurl, graalvm8, leiningen, maven, jdk, ... }:

let
  java-deps = import ./deps.nix { inherit pkgs; };
in stdenv.mkDerivation {
  pname = "rep";
  version = "0.1.2";

  src = ./.;

  buildInputs = [
    clojure
    graalvm8
    leiningen
    maven
  ] ++ map (x: x.path) java-deps.packages;

  buildPhase = ''
    rm -rf classes/
    mkdir classes/
    for jar in deps/*; do
      CLASSPATH="$jar:$CLASSPATH"
    done
    export CLASSPATH
    java -cp src:$CLASSPATH clojure.main -e "(compile 'rep.core)"
    native-image \
      -cp classes:$CLASSPATH \
      --verbose \
      --no-server \
      -J-Xmx3g \
      --report-unsupported-elements-at-runtime \
      -H:ReflectionConfigurationFiles=reflectconfig.json \
      --initialize-at-build-time \
      rep.core
  '';
  installPhase = ''
    mkdir -p $out/bin
    cp rep.core $out/bin/rep
  '';

  meta = with stdenv.lib; {
    description = "TODO: fill me in";
    homepage = https://github.com/eraserhd/rep;
    license = licenses.unlicense;
    platforms = platforms.all;
    maintainers = [ maintainers.eraserhd ];
  };
}
