{ pkgs }:
pkgs.alacritty.overrideAttrs (old: rec {
  name = "alacritty-patched";
  src = pkgs.fetchFromGitHub {
    owner = "SCOTT-HAMILTON";
    repo = "alacritty";
    rev = "6890c6d47c276e6fa84f769a3fb8dc93add72e1e";
    # If you don't know the hash, the first time, set:
    # sha256 = "0000000000000000000000000000000000000000000000000000";
    # then nix will fail the build with such an error message:
    # hash mismatch in fixed-output derivation '/nix/store/m1ga09c0z1a6n7rj8ky3s31dpgalsn0n-source':
    # wanted: sha256:0000000000000000000000000000000000000000000000000000
    # got:    sha256:173gxk0ymiw94glyjzjizp8bv8g72gwkjhacigd1an09jshdrjb4
    sha256 = "0gpacmqicbxbw3c674r6ydc0dnjqb9k06gnkxkadqijpkcnpri69";
  };
  doCheck = false;
  cargoDeps = old.cargoDeps.overrideAttrs (pkgs.lib.const {
    inherit src;
    outputHash = "0nbj4gw0qpv6l11rr2mf3sdz9a2qkgp7cfj9g7zkzzg4b53d9s6x";
    doCheck = false;
  });
})

