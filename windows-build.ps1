param(
    [string]$BuildDir = (Join-Path $PSScriptRoot "build"),
    [string]$BuildType = "Release",
    [string]$VcpkgDir = ""
)

function die {
    param([string]$message)
    if ($LASTEXITCODE -ne 0) {
        Write-Error $message
        exit 1
    }
}

function Get-VisualStudioGenerator {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $ver = & $vswhere -latest -property installationVersion
        if ($ver -match "^(\d+)") {
            switch ($matches[1]) {
                "18" { return "Visual Studio 18 2026" }
                "17" { return "Visual Studio 17 2022" }
                "16" { return "Visual Studio 16 2019" }
                "15" { return "Visual Studio 15 2017" }
            }
        }
    }
    return $null
}

function Add-DependencyToolsToPath {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -property installationPath
        if ($installPath) {
            $vcToolsDir = Join-Path $installPath "VC\Tools\MSVC"
            if (Test-Path $vcToolsDir) {
                $msvcVersions = Get-ChildItem -Directory $vcToolsDir | Sort-Object Name -Descending
                foreach ($v in $msvcVersions) {
                    $dumpbinDir = Join-Path $v.FullName "bin\Hostx64\x64"
                    $dumpbin = Join-Path $dumpbinDir "dumpbin.exe"
                    if (Test-Path $dumpbin) {
                        $env:PATH = "$dumpbinDir;$env:PATH"
                        break
                    }
                }
            }
            $llvmDir = Join-Path $installPath "VC\Tools\Llvm\bin"
            $llvmObjdump = Join-Path $llvmDir "llvm-objdump.exe"
            if (Test-Path $llvmObjdump) {
                $env:PATH = "$llvmDir;$env:PATH"
            }
        }
    }
}

$SrcDir = $PSScriptRoot
if ([string]::IsNullOrEmpty($VcpkgDir)) {
    $VcpkgDir = Join-Path $BuildDir "vcpkg"
}
$VcpkgInstalledDir = Join-Path $BuildDir "vcpkg_installed"
$CMakeBuildDir = Join-Path $BuildDir "cmake-build"

Write-Host "Build Directory: $BuildDir"
Write-Host "Build Type: $BuildType"
Write-Host "Vcpkg Directory: $VcpkgDir"
Write-Host "Vcpkg Installed Directory: $VcpkgInstalledDir"
Write-Host "CMake Build Directory: $CMakeBuildDir"

New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

if (!(Test-Path $VcpkgDir)) {
    Write-Host "Cloning vcpkg repository..."
    git clone --depth 1 https://github.com/Microsoft/vcpkg.git $VcpkgDir
    die "Failed to clone vcpkg repository"
}

if (!(Test-Path (Join-Path $VcpkgDir "vcpkg.exe"))) {
    Write-Host "Bootstrapping vcpkg..."
    & "$VcpkgDir\bootstrap-vcpkg.bat" -DisableMetrics
    die "Failed to bootstrap vcpkg"
}

Write-Host "Installing dependencies using vcpkg..."
try {
    Push-Location $SrcDir
    & "$VcpkgDir\vcpkg.exe" install --triplet=x64-windows --clean-after-build --x-install-root=$VcpkgInstalledDir
    die "Failed to install vcpkg dependencies"
} finally {
    Pop-Location
}

New-Item -ItemType Directory -Path $CMakeBuildDir -Force | Out-Null

Write-Host "Configuring with CMake..."
try {
    Push-Location $CMakeBuildDir

    # Add vcpkg installed bin directory to PATH
    $env:PATH = "$VcpkgInstalledDir\x64-windows\bin;$env:PATH"
    Add-DependencyToolsToPath

    $generator = Get-VisualStudioGenerator
    $cmakeArgs = @("-A", "x64", "-DCMAKE_BUILD_TYPE=$BuildType", "-DVCPKG_INSTALLED_DIR=$VcpkgInstalledDir", "-DVCPKG_TARGET_TRIPLET=x64-windows", "-DCMAKE_TOOLCHAIN_FILE=$VcpkgDir/scripts/buildsystems/vcpkg.cmake", $PSScriptRoot)
    if ($generator) {
        Write-Host "Detected Visual Studio Generator: $generator"
        $cmakeArgs = @("-G", "$generator") + $cmakeArgs
    } else {
        Write-Warning "Visual Studio generator not detected. Using CMake default."
    }
    & cmake $cmakeArgs
    die "Failed to configure CMake project"

    Write-Host "Building project..."
    cmake --build . --config $BuildType
    die "Failed to build project"

    Write-Host "Packaging project..."
    cpack -C $BuildType
    die "Failed to package project"
} finally {
    Pop-Location
}

Write-Host "Build completed successfully!"
