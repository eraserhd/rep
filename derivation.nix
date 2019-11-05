{ stdenv, fetchurl, graalvm8, leiningen, maven, jdk, ... }:

let
  leiningen_2_8_3 = leiningen.overrideAttrs (oldAttrs: rec {
    version = "2.8.3";
    src = fetchurl {
      url = "https://raw.github.com/technomancy/leiningen/${version}/bin/lein-pkg";
      sha256 = "1jbrm4vdvwskbi9sxvn6i7h2ih9c3nfld63nx58nblghvlcb9vwx";
    };

    jarsrc = fetchurl {
      # NOTE: This is actually a .jar, Github has issues
      url = "https://github.com/technomancy/leiningen/releases/download/${version}/${oldAttrs.pname}-${version}-standalone.zip";
      sha256 = "07kb7d84llp24l959gndnfmislnnvgpsxghmgfdy8chy7g4sy2kz";
    };

    JARNAME = "${oldAttrs.pname}-${version}-standalone.jar";
  });

in stdenv.mkDerivation {
  pname = "rep";
  version = "0.1.2";

  src = ./.;

  buildInputs = [ graalvm8 leiningen_2_8_3 maven ];

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
