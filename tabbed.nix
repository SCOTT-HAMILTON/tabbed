{ lib
, llvmPackages_11
, gitignoreSource
, autoPatchelfHook
, xorgproto
, libX11
, libXft
, libbsd
, customConfig ? null
, buildInstrumentedCoverage ? false
, patches ? []
, binutils
, gnugrep
}:
llvmPackages_11.stdenv.mkDerivation {
  name = "tabbed-20180309-patched";

  src = gitignoreSource [] ./.;

  inherit patches;

  postPatch = lib.optionalString (customConfig != null)
  ''
    cp ${builtins.toFile "config.h" customConfig} ./config.h
  '';
  
  nativeBuildInputs = [ autoPatchelfHook ];
  buildInputs = [ xorgproto libX11 libXft libbsd ];

  makeFlags = [
    "PREFIX=$(out)"
    "CC=clang"
  ] ++ lib.optional buildInstrumentedCoverage [
    "BUILD_INSTRUMENTED_COVERAGE=1"
  ];

  postInstall = ''
    readelf -a "$out/bin/tabbed" | grep llvm
  '';

  meta = with lib; {
    homepage = "https://tools.suckless.org/tabbed";
    description = "Simple generic tabbed fronted to xembed aware applications";
    license = licenses.mit;
    maintainers = with maintainers; [ shamilton ];
    platforms = platforms.linux;
  };
}
