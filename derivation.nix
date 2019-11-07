{ stdenv, pkgs, asciidoc-full, clojure, graalvm8, ... }:

let
  java-deps = import ./deps.nix { inherit pkgs; };
in stdenv.mkDerivation {
  pname = "rep";
  version = "0.1.2";

  src = ./.;

  buildInputs = [
    asciidoc-full
    clojure
    graalvm8
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
      rep.core \
      rep
    a2x -f manpage rep.1.adoc
  '';
  installPhase = ''
    mkdir -p $out/bin/ $out/share/man/man1/ $out/share/kak/plugins/
    cp rep $out/bin/
    cp rep.1 $out/share/man/man1/
    cp rc/rep.kak $out/share/kak/plugins/
  '';

  meta = with stdenv.lib; {
    description = "TODO: fill me in";
    homepage = https://github.com/eraserhd/rep;
    license = licenses.unlicense;
    platforms = platforms.all;
    maintainers = [ maintainers.eraserhd ];
  };
}
