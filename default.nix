let
  nixpkgs = import ./nixpkgs.nix;
  pkgs = import nixpkgs {
    config = {};
    overlays = [ (import ./overlay.nix) ];
  };

in pkgs.rep
