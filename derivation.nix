{ stdenv, pkgs, asciidoc-full, ... }:

stdenv.mkDerivation {
  pname = "rep";
  version = "0.2.0";

  src = ./.;

  buildInputs = [
    asciidoc-full
  ];

  postPatch = ''
    substituteInPlace rc/rep.kak --replace '$(rep' '$('"$out/bin/rep"
  '';
  makeFlags = [ "prefix=${placeholder "out"}" ];

  meta = with stdenv.lib; {
    description = "Single-shot nREPL client";
    homepage = https://github.com/eraserhd/rep;
    license = licenses.epl10;
    platforms = platforms.all;
    maintainers = [ maintainers.eraserhd ];
  };
}
