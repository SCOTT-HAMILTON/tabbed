{ lib
, gcc11Stdenv
, gitignoreSource
, autoPatchelfHook
, xorgproto
, libX11
, libXft
, libbsd
, zeromq
, customConfig ? null
, buildInstrumentedCoverage ? false
, patches ? []
, binutils
, gnugrep
}:
gcc11Stdenv.mkDerivation {
  name = "tabbed-20180309-patched";

  src = gitignoreSource [] ./.;

  inherit patches;

  postPatch = lib.optionalString (customConfig != null)
  ''
    cp ${builtins.toFile "config.h" customConfig} ./config.h
  '';
  
  nativeBuildInputs = [ autoPatchelfHook ];
  buildInputs = [ xorgproto libX11 libXft libbsd zeromq ];

  makeFlags = [
    "PREFIX=$(out)"
  ] ++ lib.optional buildInstrumentedCoverage [
    "BUILD_INSTRUMENTED_COVERAGE=1"
  ];

  meta = with lib; {
    homepage = "https://tools.suckless.org/tabbed";
    description = "Simple generic tabbed fronted to xembed aware applications";
    license = licenses.mit;
    maintainers = with maintainers; [ shamilton ];
    platforms = platforms.linux;
  };
}
