{ stdenv, fetchurl, graalvm8, leiningen, maven, jdk, ... }:

stdenv.mkDerivation {
  pname = "rep";
  version = "0.1.2";

  src = ./.;

  buildInputs = [
    graalvm8
    #leiningen_2_8_3
    leiningen
    maven
  ];

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
