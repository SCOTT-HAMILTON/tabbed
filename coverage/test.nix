let
  str_sleep_time = builtins.toString 5;
  # For extra determinism
  nixpkgs = builtins.fetchTarball {
    url = "http://github.com/NixOS/nixpkgs/archive/389249fa9b35b3071b4ccf71a3c065e7791934df.tar.gz";
    sha256 = "1z087f1m1k4pii2v2xai8n0yd3m57svgslzzbm7fwdjjzhn8g2rl";
  };
  gcc11Pkgs = import (builtins.fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/tarball/d75d0b9";
    sha256 = "14nabmj4xgamrz8nskzl595j1nkaxch1pssmw1dfr0xp0pbgf4cg";
  }) {};
  shamilton = import (builtins.fetchTarball {
    url = "https://github.com/SCOTT-HAMILTON/nur-packages/tarball/9bd7ba3";
    sha256 = "1mimljrgffmhm0hv60h9bjiiwhb069m7g1fxnss4nfr5vz1yjady";
  }) {};
  pkgs = import nixpkgs {};
  patched-alacritty = shamilton.patched-alacritty;
  instrumented-tabbed = with gcc11Pkgs; callPackage ../tabbed.nix {
    buildInstrumentedCoverage = true;
    inherit (nix-gitignore) gitignoreSource;
  };
  source = ../.;

  runTabbedAlacritty = pkgs.writeScriptBin "tabbed-alacritty" ''
    #!${pkgs.stdenv.shell}
    export TABBED_XEMBED_PORT_OPTION='--xembed-tcp-port'
    export TABBED_WORKING_DIR_OPTION='--working-directory'
    export SOURCE_DIR=/tmp
    timeout 10m ${instrumented-tabbed}/bin/tabbed -cr 2 alacritty --embed "" 1>&2 &
  '';
  makeCoverageResults = pkgs.writeScriptBin "make-coverage-results" ''
    #!${pkgs.stdenv.shell}
    killall tabbed
    ls -lh
    gcov_drv=$(cat ${gcc11Pkgs.gcc11Stdenv.cc}/bin/gcc|tail -n5|head -n1|grep -Eo "/nix/.*/gcc"|rev|cut -d/ -f3-|rev)
    lcov --gcov-tool "$gcov_drv/bin/gcov" --capture --base-directory . --directory . --output-file=tabbed-alacritty.lcov
    mkdir res
    genhtml tabbed-alacritty.lcov --output-directory res
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
        killall
        lcov
        makeCoverageResults
        patched-alacritty
        runTabbedAlacritty
        xdotool
        xlibs.xwininfo
      ];
    };

    enableOCR = true;

    testScript = ''
      import os

      start_all()

      sleep_time = int(${str_sleep_time})

      # Copy sources to tabbed directory
      machine.succeed(
          "find '${source}' -name '*.c' -exec cp {} /tmp \;"
      )
      machine.succeed(
          "find '${source}' -name '*.h' -exec cp {} /tmp \;"
      )
      machine.succeed(
          "find '${instrumented-tabbed}/share/gcno-tabbed' -name '*.gcno' -exec cp {} /tmp \;"
      )
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
      machine.succeed(
          f"xdotool mousemove --window $(xdotool getactivewindow) 500 10 click 1 click 4 click 5 click 2"
      )
      machine.sleep(sleep_time)
      machine.screenshot("screen6")

      ### Goto ~ and exit tmp tab
      machine.send_chars("cd ~")
      machine.send_key("ret")
      machine.sleep(sleep_time)
      machine.screenshot("screen7")

      ### Try to resize embedded window
      machine.succeed("xwininfo -tree -root 1>&2")
      machine.succeed(
          'xdotool windowsize $(xwininfo -tree -root|grep "Alacritty"|head -n1|grep -Eo "0x([0-9]|[a-z])* ") 500 500'
      )
      machine.sleep(sleep_time)
      machine.screenshot("screen8")

      # Close everything
      machine.send_key("alt-f4")
      machine.sleep(sleep_time)
      machine.screenshot("screen9")

      machine.succeed("make-coverage-results 1>&2")
      machine.copy_from_vm("tabbed-alacritty.lcov", "coverage_data")
      machine.copy_from_vm("res", "coverage_data")
      out_dir = os.environ.get("out", os.getcwd())
      eprint(
          'Coverage data written to "{}/coverage_data/tabbed-alacritty.lcov"'.format(out_dir)
      )
      machine.screenshot("screen10")
    '';
})
