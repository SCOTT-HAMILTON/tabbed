{ pkgs ? import <nixpkgs> {}}:
let
  lib = pkgs.lib;
  rpathLibs = with pkgs; [
    expat
    fontconfig
    freetype
    libGL
    wayland
    libxkbcommon
  ] ++ [
    xlibs.libX11
    xlibs.libXcursor
    xlibs.libXi
    xlibs.libXrandr
    xlibs.libXxf86vm
    xlibs.libxcb
  ];
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
in
with pkgs; mkShell {
  nativeBuildInputs = [ cargo rustc pkg-config fontconfig ];
  buildInputs = rpathLibs;
  propagatedBuildInputs = [
    libGL
    patched-tabbed
  ];
  shellHook = ''
      fix_alacritty() {
        patchelf --set-rpath "${lib.makeLibraryPath rpathLibs}" ./target/debug/alacritty
      }
      # tabbed -cr 2 -w "--xembed-tcp-port" ./target/debug/alacritty --embed "" 2>&1 | ./filter_output.pl 'debug-' debug_logs /dev/stdout 2>&1
  '';
}

