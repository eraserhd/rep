{
  description = "TODO: fill me in";
  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, flake-utils }:
    (flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        rep = pkgs.callPackage ./derivation.nix {};
      in {
        packages = {
          default = rep;
          inherit rep;
        };
        checks = {
          test = pkgs.runCommandNoCC "rep-test" {} ''
            mkdir -p $out
            : ${pkgs.rep}
          '';
        };
    })) // {
      overlays.default = final: prev: {
        rep = prev.callPackage ./derivation.nix {};
      };
    };
}
