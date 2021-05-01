let
  str_sleep_time = builtins.toString 5;
  # For extra determinism
  nixpkgs = builtins.fetchTarball {
    url = "http://github.com/NixOS/nixpkgs/archive/389249fa9b35b3071b4ccf71a3c065e7791934df.tar.gz";
    sha256 = "1z087f1m1k4pii2v2xai8n0yd3m57svgslzzbm7fwdjjzhn8g2rl";
  };
  shamilton = import (builtins.fetchTarball {
    url = "https://github.com/SCOTT-HAMILTON/nur-packages/tarball/9bd7ba3";
    sha256 = "1mimljrgffmhm0hv60h9bjiiwhb069m7g1fxnss4nfr5vz1yjady";
  }) {};
  pkgs = import nixpkgs {};
  patched-alacritty = shamilton.patched-alacritty;
  instrumented-tabbed = with pkgs; callPackage ../tabbed.nix {
    buildInstrumentedCoverage = true;
    inherit (nix-gitignore) gitignoreSource;
  };
  source = ../.;

  runTabbedAlacritty = pkgs.writeScriptBin "tabbed-alacritty" ''
    #!${pkgs.stdenv.shell}
    export TABBED_XEMBED_PORT_OPTION='--xembed-tcp-port'
    export TABBED_WORKING_DIR_OPTION='--working-directory'
    export LLVM_PROFILE_FILE='tabbed-alacritty-%p.profraw'
    timeout 10m ${instrumented-tabbed}/bin/tabbed -cr 2 alacritty --embed "" &
  '';
in
  import "${nixpkgs}/nixos/tests/make-test-python.nix" ({ pkgs, ...}: {
    system = "x86_64-linux";

    nodes.machine = { nodes, config, pkgs, ... }:
    {
      imports = [
        "${nixpkgs}/nixos/tests/common/user-account.nix"
        "${nixpkgs}/nixos/tests/common/x11.nix"
      ];
      environment.systemPackages = with pkgs; [
        binutils
        coreutils
        glibc
        gnugrep
        instrumented-tabbed
        llvmPackages_11.bintools
        patched-alacritty
        runTabbedAlacritty
        xdotool
      ];
    };

    enableOCR = true;

    testScript = ''
      import os

      start_all()

      sleep_time = int(${str_sleep_time})

      # Copy sources to tabbed directory
      machine.succeed("cp -r ${source} tabbed")
      machine.wait_for_x()

      machine.succeed("tabbed-alacritty")
      # machine.wait_for_text("root@machine")
      machine.sleep(sleep_time * 5)
      machine.screenshot("screen1")
      
      #### Normal Use case sequences
      ### Goto /tmp
      machine.send_chars("cd /tmp")
      machine.send_key("ret")
      machine.sleep(sleep_time)
      machine.screenshot("screen2")
      ### Open a new tab
      machine.send_key("ctrl-shift-ret")
      machine.sleep(sleep_time)
      machine.screenshot("screen3")
      ### Goto /proc
      machine.send_chars("cd /proc")
      machine.send_key("ret")
      machine.sleep(sleep_time)
      machine.screenshot("screen4")
      ### Open a new tab
      machine.send_key("ctrl-shift-ret")
      machine.sleep(sleep_time)
      machine.screenshot("screen5")
      
      ### click on middle tab
      window_width = 1000
      machine.succeed(f"xdotool windowsize $(xdotool getactivewindow) {window_width} 500")
      machine.succeed(f"xdotool mousemove --window $(xdotool getactivewindow) 500 10 click 1")
      machine.sleep(sleep_time)
      machine.screenshot("screen6")

      ### Goto ~ and exit proc tab
      machine.send_chars("cd ~")
      machine.send_key("ret")
      machine.send_chars("exit")
      machine.send_key("ret")
      machine.sleep(sleep_time)
      machine.screenshot("screen7")
      ### Goto ~ and exit tmp tab
      machine.send_chars("cd ~")
      machine.send_key("ret")
      machine.send_chars("exit")
      machine.send_key("ret")
      machine.sleep(sleep_time)
      machine.succeed("ls -lh 1>&2")

      machine.succeed(
          "llvm-profdata merge -sparse *.profraw -o tabbed-alacritty.profdata",
          "llvm-cov export ${instrumented-tabbed}/bin/tabbed -format=lcov -instr-profile=tabbed-alacritty.profdata > tabbed-alacritty.lcov",
      )
      machine.copy_from_vm("tabbed-alacritty.lcov", "coverage_data")
      machine.copy_from_vm("tabbed-alacritty.profdata", "coverage_data")
      out_dir = os.environ.get("out", os.getcwd())
      eprint(
          'Coverage data written to "{}/coverage_data/tabbed-alacritty.lcov"'.format(out_dir)
      )
      machine.screenshot("screen8")
    '';
})
