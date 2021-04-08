{ pkgs ? import <nixpkgs> {}}:
let
  lib = pkgs.lib;
  patched-tabbed = pkgs.tabbed.overrideAttrs (old: {
    name = "tabbed-20180310-patched";
    src = pkgs.fetchFromGitHub {
      owner = "SCOTT-HAMILTON";
      repo = "tabbed";
      rev = "2836d8319efe19f7e3c866ee6923d5d11d925194";
      # If you don't know the hash, the first time, set:
      # sha256 = "0000000000000000000000000000000000000000000000000000";
      # then nix will fail the build with such an error message:
      # hash mismatch in fixed-output derivation '/nix/store/m1ga09c0z1a6n7rj8ky3s31dpgalsn0n-source':
      # wanted: sha256:0000000000000000000000000000000000000000000000000000
      # got:    sha256:173gxk0ymiw94glyjzjizp8bv8g72gwkjhacigd1an09jshdrjb4
      sha256 = "1d3qdgsbbqj9ryj6rgb66r4yayfp35aa4s6hgm9cch6wwsk3dcgc";
    };
  });
  patched-alacritty = pkgs.alacritty.overrideAttrs (old: rec {
    name = "alacritty-patched";
    src = pkgs.fetchFromGitHub {
      owner = "SCOTT-HAMILTON";
      repo = "alacritty";
      rev = "230f9207e38282814cc2f463eaf726dff9d45788";
      # If you don't know the hash, the first time, set:
      # sha256 = "0000000000000000000000000000000000000000000000000000";
      # then nix will fail the build with such an error message:
      # hash mismatch in fixed-output derivation '/nix/store/m1ga09c0z1a6n7rj8ky3s31dpgalsn0n-source':
      # wanted: sha256:0000000000000000000000000000000000000000000000000000
      # got:    sha256:173gxk0ymiw94glyjzjizp8bv8g72gwkjhacigd1an09jshdrjb4
      sha256 = "0xdnli6y6pjapskb00bwsmzdhvk69pljzi81agcsipgqgd1nxk04";
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

