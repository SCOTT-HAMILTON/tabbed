{ }:
let
  pkgs = import (builtins.fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/tarball/d75d0b9";
    sha256 = "14nabmj4xgamrz8nskzl595j1nkaxch1pssmw1dfr0xp0pbgf4cg";
  }) { };
  shamilton = import (builtins.fetchTarball {
    url = "https://github.com/SCOTT-HAMILTON/nur-packages/tarball/9bd7ba3";
    sha256 = "1mimljrgffmhm0hv60h9bjiiwhb069m7g1fxnss4nfr5vz1yjady";
  }) {};
  patched-alacritty = shamilton.patched-alacritty;
  instrumented-tabbed = with pkgs; callPackage ../tabbed.nix {
    buildInstrumentedCoverage = true;
    inherit (nix-gitignore) gitignoreSource;
  };
in
with pkgs; mkShell {
  propagatedBuildInputs = [
    coreutils
    findutils
    gnugrep
    instrumented-tabbed
    lcov
    patched-alacritty
    python38Packages.gcovr
    surf
    utillinux
  ];
  shellHook = ''
    rm -rf test
    mkdir test
    cp ../*.c test
    cp ../*.h test
    find "${instrumented-tabbed}/share/gcno-tabbed" -name "*.gcno" -exec cp {} test \;
    cd test
    export SOURCE_DIR=$(pwd)
    gcov_drv=$(cat ${gcc11Stdenv.cc}/bin/gcc|tail -n5|head -n1|grep -Eo "/nix/.*/gcc"|rev|cut -d/ -f3-|rev)
    alias gcov="$gcov_drv/bin/gcov"
    run(){
      lcov --gcov-tool "$gcov_drv/bin/gcov" --capture --base-directory . --directory . --output-file=out.lcov
      rm -rf ../res 2>/dev/null
      mkdir ../res
      genhtml out.lcov --output-directory ../res
      surf ../res/index.html
    }
  '';
}

