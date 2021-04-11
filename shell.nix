{ pkgs ? import <nixpkgs> {}}:
let
  lib = pkgs.lib;
  patched-tabbed = pkgs.callPackage ./tabbed.nix { };
  patched-alacritty = import ./alacritty.nix { inherit pkgs; };
in
with pkgs; mkShell {
  propagatedBuildInputs = [
    patched-tabbed patched-alacritty
  ];
  shellHook = ''
      # tabbed -cr 2 -w "--xembed-tcp-port" ./target/debug/alacritty --embed "" 2>&1 | ./filter_output.pl 'debug-' debug_logs /dev/stdout 2>&1
  '';
}

