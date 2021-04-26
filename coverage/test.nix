let
  # For extra determinism
  nixpkgs = builtins.fetchTarball {
    url = "http://github.com/NixOS/nixpkgs/archive/d395190b24b27a65588f4539c423d9807ad8d4e7.tar.gz";
    sha256 = "0r1kj8gf97z9ydh36vmgrar1q4l9ggaqiygxjvp8jmr1948y0nh2";
  };
  pkgs = import nixpkgs {};
  # Single source of truth for all tutorial constants
  patched-alacritty = with pkgs; lib.traceValFn 
              (x: "Nixpkgs version ${lib.version}")
              (import ../alacritty.nix { inherit pkgs; });
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
        coreutils
        binutils
        file
        glibc
        instrumented-tabbed
        less
        llvmPackages_11.bintools
        gnugrep
        patched-alacritty
        runTabbedAlacritty
      ];
    };

    enableOCR = true;

    testScript = ''
      import os

      start_all()
      
      # Copy sources to tabbed directory
      machine.succeed("cp -r ${source} tabbed")
      machine.wait_for_x()

      machine.succeed(
          "echo BUILDED_IS : ${instrumented-tabbed}/bin 1>&2",
          "echo STAT_IS : $(stat ${instrumented-tabbed}/bin/tabbed) 1>&2",
          "echo STAT_IS : $(stat /run/current-system/sw/bin/tabbed) 1>&2",
          "echo FILE_IS : $(file $(readlink /run/current-system/sw/bin/tabbed)) 1>&2",
          "echo LDD_IS : $(ldd $(readlink /run/current-system/sw/bin/tabbed)) 1>&2",
          "echo READELF_IS : $(readelf -a ${instrumented-tabbed}/bin/tabbed | grep llvm) 1>&2",
      )
      machine.succeed("tabbed-alacritty")
      # machine.wait_for_text("root@machine")
      machine.sleep(5)
      machine.screenshot("screen1")
      
      #### Normal Use case sequences
      ### Goto /tmp
      machine.send_chars("cd /tmp")
      machine.send_key("ret")
      machine.sleep(5)
      machine.screenshot("screen2")
      ### Open a new tab
      machine.send_key("ctrl-shift-ret")
      machine.sleep(5)
      machine.screenshot("screen3")
      ### Goto /proc
      machine.send_chars("cd /proc")
      machine.send_key("ret")
      machine.sleep(5)
      machine.screenshot("screen4")
      ### Open a new tab
      machine.send_key("ctrl-shift-ret")
      machine.sleep(5)
      machine.screenshot("screen5")
      ### Goto ~ and exit proc tab
      machine.send_chars("cd ~")
      machine.send_key("ret")
      machine.send_chars("exit")
      machine.send_key("ret")
      machine.sleep(5)
      machine.screenshot("screen6")
      ### Goto ~ and exit tmp tab
      machine.send_chars("cd ~")
      machine.send_key("ret")
      machine.send_chars("exit")
      machine.send_key("ret")
      machine.sleep(5)
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
      machine.screenshot("screen7")
    '';
})
