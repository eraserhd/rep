{ stdenv, pkgs, asciidoc-full, graalvm8, ... }:

let
  patched_clojure = stdenv.mkDerivation {
    name = "patched-clojure-1.10.0";
    src = ./deps;
    installPhase = ''
      mkdir -p $out/share/java
      cp clojure-1.10*.jar $out/share/java/
    '';
  };

  patched_spec_alpha = stdenv.mkDerivation {
    name = "patched-spec.alpha-0.2.177";
    src = ./deps;
    installPhase = ''
      mkdir -p $out/share/java
      cp spec.alpha*.jar $out/share/java/
    '';
  };

  build-deps = map (x: x.path) (import ./deps.nix { inherit pkgs; }).packages ++ [
    patched_clojure
    patched_spec_alpha
  ];
in stdenv.mkDerivation {
  pname = "rep";
  version = "0.1.2";

  src = ./.;

  buildInputs = [
    asciidoc-full
    graalvm8
  ] ++ build-deps;

  buildPhase = ''
    rm -rf classes/
    mkdir classes/
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
    mkdir -p $out/bin/ $out/share/man/man1/ $out/share/kak/autoload/plugins/
    cp rep $out/bin/
    cp rep.1 $out/share/man/man1/
    substitute rc/rep.kak $out/share/kak/autoload/plugins/rep.kak \
      --replace '$(rep' '$('"$out/bin/rep"
  '';

  meta = with stdenv.lib; {
    description = "Single-shot nREPL client";
    homepage = https://github.com/eraserhd/rep;
    license = licenses.epl10;
    platforms = platforms.all;
    maintainers = [ maintainers.eraserhd ];
  };
}
