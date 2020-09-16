let
  nixpkgs = import ./nixpkgs.nix;
  pkgs = import nixpkgs {
    config = { allowBroken = true; };
    overlays = [ (import ./overlay.nix) ];
  };

  clj2nix = pkgs.callPackage (pkgs.fetchFromGitHub {
      owner = "hlolli";
      repo = "clj2nix";
      rev = "bc74da7531814cf894be5b618d40de8298547da9";
      sha256 = "0z5f5vs2ibhni7ydic3l5f8wy53wbwxf7pax963pcj714m3mlp47";
    }) {};
in pkgs.rep.overrideAttrs (old: {
  buildInputs = old.buildInputs ++ [ clj2nix ];
})
