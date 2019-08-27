{ pkgs ? import <nixpkgs> {} }:

let graalvm = pkgs.stdenv.mkDerivation {
  name = "graalvm";
  version = "1.0rc16";
  src = pkgs.fetchurl {
    url = "https://github.com/oracle/graal/releases/download/vm-1.0.0-rc16/graalvm-ce-1.0.0-rc16-linux-amd64.tar.gz";
    sha256 = "017990kdhxf8yw530h3ik3hscyklzzrr5nmcj95jsdg2m6ls4cr3";
  };

  buildInputs = with pkgs; [ patchelf zlib ];

  installPhase = ''
    find bin include jre lib man -type d -exec mkdir -p "$out/{}" \;
    find bin include jre lib man -type f -exec cp "{}" "$out/{}" \;
    interp="$(cat $NIX_CC/nix-support/dynamic-linker)"
    rpath="$libPath"
    find "$out/lib" -type d |while read libdir; do
      if [[ -f "$libdir/"*.so ]]; then
        rpath="$rpath:$libdir"
      fi
    done
    echo rpath = $rpath
    ls -l "$rpath"
    find "$out" -type f -executable -exec patchelf --set-interpreter "$interp" --set-rpath "$rpath" "{}" \;
  '';

  libPath = (pkgs.stdenv.lib.makeLibraryPath [pkgs.zlib]);
}; in

pkgs.mkShell {
  buildInputs = with pkgs; [
    graalvm
  ];
}
