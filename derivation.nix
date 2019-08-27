{ stdenv, ... }:

stdenv.mkDerivation {
  pname = "rep";
  version = "0.1.2";

  src = ./.;

  meta = with stdenv.lib; {
    description = "TODO: fill me in";
    homepage = https://github.com/eraserhd/rep;
    license = licenses.publicDomain;
    platforms = platforms.all;
    maintainers = [ maintainers.eraserhd ];
  };
}
