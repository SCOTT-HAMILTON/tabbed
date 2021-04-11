{ pkgs ? import <nixpkgs> {}}:
let
  patched-alacritty = import ../alacritty.nix { inherit pkgs; };
  patched-tabbed = pkgs.callPackage ../tabbed.nix { };
  instrumented-tabbed = patched-tabbed.overrideAttrs (old: rec {
    makeFlags = (old.makeFlags or []) ++ [
      "BUILD_INSTRUMENTED_COVERAGE=1"
    ];
  });
in
with pkgs; mkShell {
  propagatedBuildInputs = [
    patched-alacritty
    llvmPackages_11.bintools
    instrumented-tabbed
    less
  ];
  shellHook = ''
    run_coverage(){
      export TABBED_XEMBED_PORT_OPTION="--xembed-tcp-port"
      export TABBED_WORKING_DIR_OPTION="--working-directory"
      LLVM_PROFILE_FILE="tabbed-alacritty-%p.profraw" tabbed -cr 2 alacritty --embed ""
    }
    make_report(){
      llvm-profdata merge -sparse *.profraw -o tabbed-alacritty.profdata
      llvm-cov show ${instrumented-tabbed}/bin/tabbed -use-color -path-equivalence=/build/tabbed,.. -instr-profile=tabbed-alacritty.profdata | less -R
    }
  '';
}

