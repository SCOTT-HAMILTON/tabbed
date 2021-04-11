let
  # For extra determinism
  nixpkgs = <nixpkgs>;
  pkgs = import nixpkgs {};
  # Single source of truth for all tutorial constants
  patched-alacritty = import ../alacritty.nix { inherit pkgs; };
  instrumented-tabbed = pkgs.callPackage ../tabbed.nix {
    buildInstrumentedCoverage = true;
  };
  source = ../.;

  runTabbedAlacritty = pkgs.writeScriptBin "tabbed-alacritty" ''
    #!${pkgs.stdenv.shell}
    export TABBED_XEMBED_PORT_OPTION='--xembed-tcp-port'
    export TABBED_WORKING_DIR_OPTION='--working-directory'
    export LLVM_PROFILE_FILE='tabbed-alacritty-%p.profraw'
    timeout 5s tabbed -cr 2 alacritty --embed "" &
  '';
in
  import "${nixpkgs}/nixos/tests/make-test-python.nix" ({ pkgs, ...}: {
    system = "x86_64-linux";

    nodes.machine = { nodes, config, pkgs, ... }:
      let user = nodes.machine.config.users.users.alice;
    in {
      imports = [
        "${nixpkgs}/nixos/tests/common/user-account.nix"
        "${nixpkgs}/nixos/tests/common/x11.nix"
      ];
      environment.systemPackages = with pkgs; [
        patched-alacritty
        llvmPackages_11.bintools
        instrumented-tabbed
        runTabbedAlacritty
        less
        coreutils
      ];
      test-support.displayManager.auto.user = user.name;
    };

    enableOCR = true;

    testScript = ''
      import os

      start_all()
      
      # Copy sources to tabbed directory
      machine.succeed("cp -r ${source} tabbed")
      machine.wait_for_x()

      machine.succeed("su alice -c 'tabbed-alacritty'")
      machine.sleep(2)
      machine.screenshot("window1")
      machine.wait_for_text("alice@machine")
      out_dir = os.environ.get("out", os.getcwd())
      machine.succeed(
          "llvm-profdata merge -sparse *.profraw -o tabbed-alacritty.profdata",
          "llvm-cov export ${instrumented-tabbed}/bin/tabbed -format=lcov -instr-profile=tabbed-alacritty.profdata > tabbed-alacritty.lcov",
      )
      machine.copy_from_vm("tabbed-alacritty.lcov", "coverage_data")
      machine.copy_from_vm("tabbed-alacritty.profdata", "coverage_data")
      eprint(
          'Coverage data written to "{}/coverage_data/tabbed-alacritty.lcov"'.format(out_dir)
      )
      machine.screenshot("window2")
    '';
})
