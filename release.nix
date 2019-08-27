{ nixpkgs ? (import ./nixpkgs.nix), ... }:
let
  pkgs = import nixpkgs { config = {}; };
  rep = pkgs.callPackage ./derivation.nix {};
in {
  test = pkgs.runCommandNoCC "rep-test" {} ''
    true
  '';
}