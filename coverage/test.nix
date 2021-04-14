let
  # For extra determinism
  # nixpkgs = builtins.fetchTarball {
  #   url = "http://github.com/mroi/nixpkgs/archive/839559b486d4c4dbd911a0f13a3b2d319b942cd5.tar.gz";
  #   sha256 = "1g3d63ad09pssniszbjiz618rmrm2g7mj6pccrq5jll2zxzsadnz";
  # };
  nixpkgs = builtins.fetchTarball {
    url = "http://github.com/SCOTT-HAMILTON/NixPkgs/archive/26bc66057a6041b2f30419b2abbb0dca4f456149.tar.gz";
    sha256 = "1br29pj4c02jalfsz578nvam2w9jlvpw0945l8is98gif6gwai18";
  };
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
    timeout 1m tabbed -cr 2 alacritty --embed "" &
  '';
in
  import "${nixpkgs}/nixos/tests/make-test-python.nix" ({ pkgs, ...}: {
    system = "x86_64-linux";
    configuration = {
      virtualisation.qemu.pkgs = import nixpkgs { system = "x86_64-darwin";};
    };

    nodes.machine = { nodes, config, pkgs, ... }:
      let user = nodes.machine.config.users.users.alice;
    in {
      imports = [
        "${nixpkgs}/nixos/tests/common/user-account.nix"
        "${nixpkgs}/nixos/tests/common/x11.nix"
      ];
      environment.systemPackages = with pkgs; [
        # patched-alacritty
        alacritty
        llvmPackages_11.bintools
        # instrumented-tabbed
        tabbed
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
      
      #### Normal Use case sequences
      ### Goto /tmp
      machine.send_chars("cd /tmp")
      machine.send_key("ret")
      machine.sleep(2)
      machine.screenshot("tabbedtmp")
      ### Open a new tab
      # machine.send_key("ctrl-shift-ret")
      # machine.sleep(2)
      # machine.screenshot("tabbedtmptab")
      ### Goto /proc
      machine.send_chars("cd /proc")
      machine.send_key("ret")
      machine.sleep(2)
      machine.screenshot("tabbedproc")
      ### Open a new tab
      # machine.send_key("ctrl-shift-ret")
      # machine.sleep(2)
      # machine.screenshot("tabbedproctab")
      ### Goto ~ and exit proc tab
      machine.send_chars("cd ~")
      machine.send_key("ret")
      machine.send_chars("exit")
      machine.send_key("ret")
      machine.sleep(2)
      machine.screenshot("tabbedproctabexit")
      ### Goto ~ and exit tmp tab
      machine.send_chars("cd ~")
      machine.send_key("ret")
      machine.send_chars("exit")
      machine.send_key("ret")
      machine.sleep(2)

      # machine.succeed(
      #     "llvm-profdata merge -sparse *.profraw -o tabbed-alacritty.profdata",
      #     "llvm-cov export ${instrumented-tabbed}/bin/tabbed -format=lcov -instr-profile=tabbed-alacritty.profdata > tabbed-alacritty.lcov",
      # )
      # machine.copy_from_vm("tabbed-alacritty.lcov", "coverage_data")
      # machine.copy_from_vm("tabbed-alacritty.profdata", "coverage_data")
      # out_dir = os.environ.get("out", os.getcwd())
      # eprint(
      #     'Coverage data written to "{}/coverage_data/tabbed-alacritty.lcov"'.format(out_dir)
      # )
      machine.screenshot("window2")
    '';
})
