{
  description = "Build the TEN Framework tman CLI from this checkout";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
    ten-gn = {
      url = "github:TEN-framework/ten_gn/0f293df9fcca0e2fd08568fb1044281dd894b0ca";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      ten-gn,
    }:
    let
      lib = nixpkgs.lib;
      systems = [
        "aarch64-darwin"
      ];
      forAllSystems = lib.genAttrs systems;
      versionText = builtins.readFile ./core/src/ten_manager/src/version.rs;
      versionLine =
        lib.findFirst (line: lib.hasPrefix "pub const VERSION: &str = \"" line)
          (throw "Unable to find the tman version in core/src/ten_manager/src/version.rs")
          (lib.splitString "\n" versionText);
      version = lib.removeSuffix "\";" (lib.removePrefix "pub const VERSION: &str = \"" versionLine);
      frontendNodeModulesHash = {
        "aarch64-darwin" = "sha256-WXmT05MVubruw33XQr2EUDXLzvJlTt9nHwAgyNLikaM=";
      };
      mkTman =
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          tgnPython = pkgs.python3.withPackages (ps: [
            ps.python-dotenv
          ]);
          targetOs =
            if pkgs.stdenv.hostPlatform.isDarwin then
              "mac"
            else if pkgs.stdenv.hostPlatform.isLinux then
              "linux"
            else
              throw "Unsupported platform: ${system}";
          targetCpu =
            if pkgs.stdenv.hostPlatform.isAarch64 then
              "arm64"
            else if pkgs.stdenv.hostPlatform.isx86_64 then
              "x64"
            else
              throw "Unsupported architecture: ${pkgs.stdenv.hostPlatform.system}";
          tenUtilsStatic = pkgs.stdenv.mkDerivation {
            pname = "ten-utils-static";
            inherit version;
            src = self;
            dontConfigure = true;
            nativeBuildInputs = [
              pkgs.clang
              pkgs.cmake
              tgnPython
            ]
            ++ lib.optionals pkgs.stdenv.hostPlatform.isDarwin [
              pkgs.cctools
            ];
            buildPhase = ''
              runHook preBuild
              ln -s ${ten-gn}/.gnfiles .gnfiles
              ln -s ${ten-gn}/.gnfiles/.gn .gn
              ${ten-gn}/tgn gen ${targetOs} ${targetCpu} release -- ten_enable_ten_rust=false ten_enable_ten_manager=false ten_enable_tests=false ten_enable_tests_cleanup=false ten_force_mingw_for_go_binding=false
              ${ten-gn}/tgn build:ten_utils_combined_static ${targetOs} ${targetCpu} release
              runHook postBuild
            '';
            installPhase = ''
              runHook preInstall
              install -D out/${targetOs}/${targetCpu}/gen/core/src/ten_utils/libten_utils_static.a \
                $out/lib/libten_utils_static.a
              runHook postInstall
            '';
          };
          frontendNodeModules = pkgs.stdenvNoCC.mkDerivation {
            pname = "designer-frontend-node_modules";
            inherit version;
            src = self;
            sourceRoot = "source/core/src/ten_manager/designer_frontend";
            impureEnvVars = lib.fetchers.proxyImpureEnvVars ++ [
              "GIT_PROXY_COMMAND"
              "SOCKS_SERVER"
            ];
            nativeBuildInputs = [
              pkgs.bun
              pkgs.writableTmpDirAsHomeHook
            ];
            dontConfigure = true;
            buildPhase = ''
              runHook preBuild
              export BUN_INSTALL_CACHE_DIR=$(mktemp -d)
              bun install --frozen-lockfile --no-progress
              runHook postBuild
            '';
            installPhase = ''
              runHook preInstall
              mkdir -p $out
              cp -R node_modules $out/
              runHook postInstall
            '';
            dontFixup = true;
            outputHash = frontendNodeModulesHash.${pkgs.stdenv.hostPlatform.system};
            outputHashAlgo = "sha256";
            outputHashMode = "recursive";
          };
          frontendDist = pkgs.stdenvNoCC.mkDerivation {
            pname = "designer-frontend-dist";
            inherit version;
            src = self;
            sourceRoot = "source/core/src/ten_manager/designer_frontend";
            nativeBuildInputs = [
              pkgs.bun
              pkgs.nodejs
            ];
            configurePhase = ''
              runHook preConfigure
              cp -R ${frontendNodeModules}/node_modules .
              runHook postConfigure
            '';
            buildPhase = ''
              runHook preBuild
              bun run build
              runHook postBuild
            '';
            installPhase = ''
              runHook preInstall
              mkdir -p $out
              cp -R dist $out/
              runHook postInstall
            '';
          };
        in
        pkgs.rustPlatform.buildRustPackage {
          pname = "tman";
          inherit version;
          src = self;
          sourceRoot = "source/core/src/ten_manager";
          cargoLock.lockFile = ./core/src/ten_manager/Cargo.lock;
          cargoBuildFlags = [
            "--bin"
            "tman"
          ];
          doCheck = false;
          nativeBuildInputs = [
            pkgs.clang
            pkgs.cmake
            pkgs.pkg-config
            pkgs.python3
          ];
          buildInputs = [
            tenUtilsStatic
          ]
          ++ lib.optionals pkgs.stdenv.hostPlatform.isDarwin [
            pkgs.libiconv
          ];
          preBuild = ''
            cp -R ${frontendDist}/dist designer_frontend/dist
            export TEN_UTILS_LIBRARY_PATH=${tenUtilsStatic}/lib
          '';
        };
      mkApp =
        system:
        let
          tman = mkTman system;
        in
        {
          type = "app";
          program = "${tman}/bin/tman";
        };
    in
    {
      packages = forAllSystems (system: {
        default = mkTman system;
        tman = mkTman system;
      });

      apps = forAllSystems (system: {
        default = mkApp system;
        tman = mkApp system;
      });

      checks = forAllSystems (system: {
        tman = mkTman system;
      });
    };
}
