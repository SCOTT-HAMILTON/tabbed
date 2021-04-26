{ pkgs ? import <nixpkgs> {}}:
let
  patched-alacritty = import ../alacritty.nix { inherit pkgs; };
  instrumented-tabbed = with pkgs; callPackage ../tabbed.nix {
    buildInstrumentedCoverage = true;
    inherit (nix-gitignore) gitignoreSource;
  };
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
    show_report(){
      llvm-cov show ${instrumented-tabbed}/bin/tabbed -use-color -path-equivalence=/build/tabbed,.. -instr-profile=$1 | less -R
    }
    show_test_report(){
      show_report "result/coverage_data/tabbed-alacritty.profdata"
    }
    make_report(){
      llvm-profdata merge -sparse *.profraw -o tabbed-alacritty.profdata
      llvm-cov show ${instrumented-tabbed}/bin/tabbed -use-color -path-equivalence=/build/tabbed,.. -instr-profile=tabbed-alacritty.profdata | less -R
    }
    export_report_to_lcov(){
      llvm-cov export ${instrumented-tabbed}/bin/tabbed -format=lcov -instr-profile=tabbed-alacritty.profdata > tabbed-alacritty.lcov
    }
  '';
}

