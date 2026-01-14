{ stdenv, cmake, pkg-config, wxGTK32 }:

stdenv.mkDerivation {
  pname = "bytemusehq";
  version = "0.1.0";

  src = ./.;

  nativeBuildInputs = [ cmake pkg-config ];
  buildInputs = [ wxGTK32 ];

  meta = with stdenv.lib; {
    description = "wxWidgets text editor with directory tree and styled text control";
    license = licenses.mit;
    platforms = platforms.linux ++ platforms.darwin;
  };
}
