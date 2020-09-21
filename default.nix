let
  nixpkgs = import ./nixpkgs.nix;
  pkgs = import nixpkgs {
    config = { allowBroken = true; };
    overlays = [ (import ./overlay.nix) ];
  };

in pkgs.rep
