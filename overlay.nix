self: super: {

  # This actually works, but Nixpkgs was wrong
  fop = super.fop.overrideAttrs (old: {
    meta = old.meta // { platforms = super.stdenv.lib.platforms.all; };
  });

  rep = super.callPackage ./derivation.nix {
    fetchFromGitHub = args: ./.;
  };
}
