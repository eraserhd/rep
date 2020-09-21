let
  nixpkgs = import ./nixpkgs.nix;
  pkgs = import nixpkgs {
    config = { allowBroken = true; };
    overlays = [ (import ./overlay.nix) ];
  };

  clj2nix = pkgs.callPackage (pkgs.fetchFromGitHub {
      owner = "hlolli";
      repo = "clj2nix";
      rev = "de55ca72391bdadcdcbdf40337425d94e55162cb";
      sha256 = "0bsq0b0plh6957zy9gl2g6hq8nhjkln4sn9lgf3yqbwz8i1z5a4a";
    }) {};
in pkgs.rep.overrideAttrs (old: {
  buildInputs = old.buildInputs ++ [ clj2nix ];
})
