let
  pkgs = import <nixpkgs> {};
  str_sleep_time = builtins.toString 5;
  # For extra determinism
  # shamilton = import /home/scott/GIT/nur-packages { localUsage = true; inherit pkgs;};
  shamilton = import (builtins.fetchTarball {
    url = "https://github.com/SCOTT-HAMILTON/nur-packages/tarball/e1ea639";
    sha256 = "08vxq12d8j3zlkph6974sc2ahqchksw117y6ya6cmh5617yzcl10";
  }) {};
  patched-alacritty = shamilton.patched-alacritty;
  instrumented-tabbed = pkgs.callPackage ../tabbed.nix {
    buildInstrumentedCoverage = true;
    inherit (pkgs.nix-gitignore) gitignoreSource;
  };
  source = ../.;

  runPrepareCov = pkgs.writeScriptBin "prepare-coverage" ''
    #!${pkgs.stdenv.shell}
    # Copy sources to tabbed directory
    find '${source}' -name '*.c' -exec cp {} /tmp \;
    find '${source}' -name '*.h' -exec cp {} /tmp \;
    find '${instrumented-tabbed}/share/gcno-tabbed' -name '*.gcno' -exec cp {} /tmp \;
  '';

  runTabbedAlacritty = pkgs.writeScriptBin "tabbed-alacritty" ''
    #!${pkgs.stdenv.shell}
    export TABBED_XEMBED_PORT_OPTION='--xembed-tcp-port'
    export TABBED_WORKING_DIR_OPTION='--working-directory'
    export SOURCE_DIR=/tmp
    export XDG_RUNTIME_DIR=$(mktemp -d)
    echo "XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
    timeout 10m ${instrumented-tabbed}/bin/tabbed -g 500x500 -cr 2 alacritty --embed "" 1>&2 &
  '';

  runAtomSpawn = pkgs.writeScriptBin "atom-spawn" ''
    #!${pkgs.stdenv.shell}
    main_id=$(xwininfo -tree -root|grep tabbed|head -n1|egrep -o "0x([0-9]|[a-z])* ")
    fork_id=$(xwininfo -children -id $main_id|grep Alacritty|egrep -o "0x([0-9]|[a-z])* ")
    echo "Main Id : $main_id"
    echo "Fork Id : $fork_id"
    xprop -id $main_id -f _TABBED_SELECT_TAB 8s -set _TABBED_SELECT_TAB $fork_id
  '';
  
  makeCoverageResults = pkgs.writeScriptBin "make-coverage-results" ''
    #!${pkgs.stdenv.shell}
    killall tabbed
    ls -lh
    gcov_drv=$(cat ${pkgs.gcc14Stdenv.cc}/bin/gcc|tail -n5|head -n1|grep -Eo "/nix/.*/gcc"|rev|cut -d/ -f3-|rev)
    lcov --gcov-tool "$gcov_drv/bin/gcov" --capture --base-directory . --directory . --output-file=tabbed-alacritty.lcov
    mkdir res
    genhtml tabbed-alacritty.lcov --output-directory res
  '';
in
  import "${<nixpkgs>}/nixos/tests/make-test-python.nix" ({ pkgs, ...}: {
    # system = "x86_64-linux";
    name = "tabbed-test";
    skipLint = true;

    nodes.machine = { nodes, config, pkgs, ... }:
    {
      imports = [
        "${<nixpkgs>}/nixos/tests/common/user-account.nix"
        "${<nixpkgs>}/nixos/tests/common/x11.nix"
      ];
      environment.systemPackages = with pkgs; [
        screen
        binutils
        coreutils
        glibc
        gnugrep
        instrumented-tabbed
        killall
        lcov
        makeCoverageResults
        patched-alacritty
        runPrepareCov
        runAtomSpawn
        runTabbedAlacritty
        xdotool
        xorg.xwininfo
      ];
    };

    enableOCR = true;

    testScript = ''
      import os

      start_all()

      def focus_tabbed():
        ## Focus newly opened window
        machine.succeed('xdotool windowfocus $(xdotool search "tabbed" 2>/dev/null | head -n1 | tail -n1)')
      

      sleep_time = int(${str_sleep_time})
      
      machine.succeed("${runPrepareCov}/bin/prepare-coverage")
      machine.wait_for_x()
      machine.succeed("${runTabbedAlacritty}/bin/tabbed-alacritty")
      machine.wait_for_text("root@machine")
      focus_tabbed()
      machine.screenshot("screen0")

      #### Normal Use case sequences
      ### Goto /tmp
      machine.send_key("ctrl-shift-ret")
      machine.sleep(sleep_time)
      focus_tabbed()
      machine.screenshot("screen1")
      machine.send_chars("cd /tmp")
      machine.send_key("ret")
      machine.sleep(sleep_time)
      machine.screenshot("screen2")
      ### Open a new tab
      machine.send_key("ctrl-shift-ret")
      machine.sleep(sleep_time)
      focus_tabbed()
      machine.screenshot("screen3")
      ### Goto /proc
      machine.send_chars("cd /proc")
      machine.send_key("ret")
      machine.sleep(sleep_time)
      machine.screenshot("screen4")
      ### Open a new tab
      machine.send_key("ctrl-shift-ret")
      machine.sleep(sleep_time)
      focus_tabbed()
      machine.screenshot("screen5")
      
      ### click on middle tab
      window_width = 1000
      machine.succeed(f"xdotool windowsize $(xdotool getactivewindow) {window_width} 500")
      machine.succeed(
         "xdotool mousemove --window $(xdotool getactivewindow) 500 10 click 1 click 4 click 5 click 2"
      )
      machine.sleep(sleep_time)
      machine.screenshot("screen6")

      ### Show left and right arrows
      machine.succeed("xdotool windowsize $(xdotool getactivewindow) 200 500")
      machine.sleep(sleep_time)
      machine.send_key("ctrl-shift-l")
      machine.sleep(sleep_time)
      machine.screenshot("screen7")

      ### Fullscreen
      machine.send_key("f11")
      machine.sleep(sleep_time)
      machine.screenshot("screen8")

      ### Rotate right and move tab left
      machine.send_key("ctrl-shift-l")
      machine.sleep(sleep_time)
      machine.send_key("ctrl-shift-h")
      machine.sleep(sleep_time)
      machine.screenshot("screen9")

      ### Focus urgent
      machine.send_key("ctrl-u")
      machine.sleep(sleep_time)
      machine.screenshot("screen10")

      ### Goto ~ and exit tmp tab
      machine.send_chars("cd ~")
      machine.send_key("ret")
      machine.sleep(sleep_time)
      machine.screenshot("screen11")

      ### Try to resize embedded window
      machine.send_key("f11")
      machine.succeed("xdotool windowsize $(xdotool search 'tabbed') 500 500")
      machine.sleep(sleep_time)
      machine.screenshot("screen12")

      ### Close everything
      machine.send_key("alt-f4")
      machine.sleep(sleep_time)
      machine.screenshot("screen13")

      machine.succeed("${makeCoverageResults}/bin/make-coverage-results 1>&2")
      machine.copy_from_vm("tabbed-alacritty.lcov", "coverage_data")
      machine.copy_from_vm("res", "coverage_data")
      out_dir = os.environ.get("out", os.getcwd)
      print(
          'Coverage data written to "{}/coverage_data/tabbed-alacritty.lcov"'.format(out_dir)
      )
      machine.screenshot("screen14")
    '';
})
