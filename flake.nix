{
  description = "wxWidgets Text Editor with Tree and Styled Editor";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "bytemusehq";
          version = "0.1.0";

          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
          ];

          buildInputs = with pkgs; [
            wxGTK32
          ];

          cmakeFlags = [ ];

          meta = with pkgs.lib; {
            description = "wxWidgets text editor with directory tree and styled text control";
            license = licenses.mit;
            platforms = platforms.linux ++ platforms.darwin;
          };
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            cmake
            pkg-config
            wxGTK32
            gcc
            gdb
            clang-tools
          ];
        };
      }
    );
}
