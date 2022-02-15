{
  #description = "a toolset to manage and build `pk3` or `dpk` source directories";

  inputs = {
    nixpkgs.url = "flake:nixpkgs";
  };

  outputs = { self, nixpkgs }:
    let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;
    in {

      packages.x86_64-linux.quake-tools =
        pkgs.stdenv.mkDerivation {
          name = "quake-tools";

          src = pkgs.lib.cleanSource ./.;

          cmakeFlags = [
            "-DGIT_VERSION=nix" # meh
            "-DDOWNLOAD_GAMEPACKS=OFF"
            "-DBUNDLE_LIBRARIES=OFF"
            "-DBUILD_CRUNCH=OFF"
            "-DBUILD_DAEMONMAP=OFF"
            "-DBUILD_RADIANT=OFF"
            "-DBUILD_TOOLS=ON"
            "-DFHS_INSTALL=ON"
          ];

          buildInputs = with pkgs; [
            pkg-config gtk2 glib libwebp libxml2 minizip
          ];
          nativeBuildInputs = with pkgs; [
            cmake subversion unzip
            python3 python38Packages.pyyaml
          ];

          postInstall = "rm -r $out/share";
        };

    };
}
