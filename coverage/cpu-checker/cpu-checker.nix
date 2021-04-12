{ stdenv
, lib
, fetchbzr
}:
stdenv.mkDerivation {
  pname = "cpu-checker";
  version = "2011-11-18";

  src = fetchbzr {
    url = "https://code.launchpad.net/~cpu-checker-dev/cpu-checker/trunk";
    rev = "41";
    sha256 = "03blmihzilgcfvd5lw0jzjcm7rfnyx1jy5ynhbbiy19p8lcr55s7";
  };

  postPatch = ''
    substituteInPlace Makefile \
      --replace "/usr" ""
  '';

  makeFlags = [
    "DESTDIR=$(out)"
  ];

  meta = with lib; {
    homepage = "https://tools.suckless.org/tabbed";
    description = "Simple generic tabbed fronted to xembed aware applications";
    license = licenses.mit;
    maintainers = with maintainers; [ shamilton ];
    platforms = platforms.linux;
  };
}
