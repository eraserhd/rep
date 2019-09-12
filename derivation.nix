{ stdenv, leiningen, graalvm8, ... }:

stdenv.mkDerivation {
  pname = "rep";
  version = "0.1.2";

  src = ./.;

  buildInputs = [ leiningen graalvm8 ];

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
