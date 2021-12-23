{ pkgs ? import <nixpkgs> {}}:
let
  lib = pkgs.lib;
  shamilton = import (builtins.fetchTarball {
    url = "https://github.com/SCOTT-HAMILTON/nur-packages/tarball/4c164c6";
    sha256 = "1ir3414wkc44lq335ziiqxk7vwsdh0wb4lqprki58ndppyvp8wsb";
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

