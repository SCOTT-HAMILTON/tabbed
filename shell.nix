{ pkgs ? import <nixpkgs> {}}:
let
  lib = pkgs.lib;
  shamilton = import (builtins.fetchTarball {
    url = "https://github.com/SCOTT-HAMILTON/nur-packages/tarball/9bd7ba3";
    sha256 = "1mimljrgffmhm0hv60h9bjiiwhb069m7g1fxnss4nfr5vz1yjady";
  }) {};
  patched-tabbed = with pkgs; callPackage ./tabbed.nix {
    inherit (nix-gitignore) gitignoreSource;
  };
  patched-alacritty = shamilton.patched-alacritty;
in
with pkgs; mkShell {
  propagatedBuildInputs = [
    patched-alacritty
    patched-tabbed
  ];
  shellHook = ''
      run(){
        tabbed -cr 2 -x "--xembed-tcp-port" -w "--working-directory" alacritty --embed ""
      }
  '';
}

