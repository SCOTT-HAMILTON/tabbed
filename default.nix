let pkgs = import (builtins.fetchTarball {
    url = "http://github.com/SCOTT-HAMILTON/nixpkgs/archive/19b1ae0eaf05358f54d6432192f102ea44b6d52b.tar.gz";
    sha256 = "1fh3l88ch5q2fygkp9rfkk35v0jvkqgdzy51lyxq46nh8cwn6p9r";
  }) { };
in with pkgs;
callPackage ./tabbed.nix {
  inherit (nix-gitignore) gitignoreSource;
  buildInstrumentedCoverage = true;
}
