{ stdenv, lib, asciidoc-full }:

stdenv.mkDerivation rec {
  pname = "rep";
  version = "0.2.2";

  src = ./.;

  nativeBuildInputs = [
    asciidoc-full
  ];

  postPatch = ''
    substituteInPlace rc/rep.kak --replace '$(rep' '$('"$out/bin/rep"
  '';
  makeFlags = [ "prefix=$(out)" ];

  meta = with lib; {
    description = "Single-shot nREPL client";
    homepage = "https://github.com/eraserhd/rep";
    license = licenses.epl10;
    platforms = platforms.all;
    maintainers = [ maintainers.eraserhd ];
  };
}
