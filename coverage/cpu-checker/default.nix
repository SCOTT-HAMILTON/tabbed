{
  pkgs ? import (builtins.fetchTarball {
    url = "http://github.com/NixOS/nixpkgs/archive/d395190b24b27a65588f4539c423d9807ad8d4e7.tar.gz";
    sha256 = "0r1kj8gf97z9ydh36vmgrar1q4l9ggaqiygxjvp8jmr1948y0nh2";
  }) {}
}:
with pkgs; callPackage ./cpu-checker.nix {}
