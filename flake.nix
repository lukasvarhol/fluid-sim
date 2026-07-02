{
  description = "Fluid-sim development environment for working on
                 NVIDIA RTX 5060Ti GPU (blackwell architecture)";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = {nixpkgs, ... }:
    let
      system = "x86_64-linux";
      # pkgs = nixpkgs.legacyPackages.${system};
      pkgs = import nixpkgs { system = "x86_64-linux"; config.allowUnfree = true; };
    in with pkgs; {
      packages.${system} = rec {
        hello = pkgs.hello;
        default = hello;
      };
      devShells.${system} = rec {
        bash = mkShell {
          buildInputs = [
            cudatoolkit
            gcc
            cmake
            wayland-scanner
            pkg-config
            wayland
            libxkbcommon
            libffi
            libx11
            libxrandr
            libxinerama
            libxcursor
            libxi
            libGL
            onetbb
          ];
          shellHook = ''
            export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib/wsl/lib
         '';
        };
        default = bash;
      };
    };
}

