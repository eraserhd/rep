{ stdenv, pkgs, asciidoc-full, ... }:

stdenv.mkDerivation {
  pname = "rep";
  version = "0.2.0";

  src = ./.;

  buildInputs = [
    asciidoc-full
  ];

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
