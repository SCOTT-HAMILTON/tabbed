{ pkgs ? import <nixpkgs> {}}:
let
  lib = pkgs.lib;
  patched-tabbed = pkgs.tabbed.overrideAttrs (old: {
    name = "tabbed-20180310-patched";
    src = pkgs.fetchFromGitHub {
      owner = "SCOTT-HAMILTON";
      repo = "tabbed";
      rev = "40d71347cb42dbfc68f636ca68120e46c7d4d520";
      # If you don't know the hash, the first time, set:
      # sha256 = "0000000000000000000000000000000000000000000000000000";
      # then nix will fail the build with such an error message:
      # hash mismatch in fixed-output derivation '/nix/store/m1ga09c0z1a6n7rj8ky3s31dpgalsn0n-source':
      # wanted: sha256:0000000000000000000000000000000000000000000000000000
      # got:    sha256:173gxk0ymiw94glyjzjizp8bv8g72gwkjhacigd1an09jshdrjb4
      sha256 = "1vdmggb4dywkm2r8ghsr48spdi49gpzd7l76m6xwl4dd791gka6c";
    };
  });
  patched-alacritty = pkgs.alacritty.overrideAttrs (old: rec {
    name = "alacritty-patched";
    src = pkgs.fetchFromGitHub {
      owner = "SCOTT-HAMILTON";
      repo = "alacritty";
      rev = "9f7323dc0d034b9f2babd77c710073a47da22e9b";
      # If you don't know the hash, the first time, set:
      # sha256 = "0000000000000000000000000000000000000000000000000000";
      # then nix will fail the build with such an error message:
      # hash mismatch in fixed-output derivation '/nix/store/m1ga09c0z1a6n7rj8ky3s31dpgalsn0n-source':
      # wanted: sha256:0000000000000000000000000000000000000000000000000000
      # got:    sha256:173gxk0ymiw94glyjzjizp8bv8g72gwkjhacigd1an09jshdrjb4
      sha256 = "08c0zhwb7nfwwfdv63gxv32crg8nascwa9iwk5rsbrsfx57fv9ja";
    };
    doCheck = false;
    cargoDeps = old.cargoDeps.overrideAttrs (pkgs.lib.const {
      inherit src;
      outputHash = "0nbj4gw0qpv6l11rr2mf3sdz9a2qkgp7cfj9g7zkzzg4b53d9s6x";
      doCheck = false;
    });
  });
in
with pkgs; mkShell {
  propagatedBuildInputs = [
    patched-tabbed patched-alacritty
  ];
  shellHook = ''
      # tabbed -cr 2 -w "--xembed-tcp-port" ./target/debug/alacritty --embed "" 2>&1 | ./filter_output.pl 'debug-' debug_logs /dev/stdout 2>&1
  '';
}

