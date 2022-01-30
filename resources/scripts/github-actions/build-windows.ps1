$os = $args[0]
$webengine = $args[1]

echo "We are building for MS Windows."
echo "OS: $os; WebEngine: $webengine"

$git_revlist = git rev-list --tags --max-count=1
$git_tag = git describe --tags $git_revlist
$git_revision = git rev-parse --short HEAD
$old_pwd = $pwd.Path

# Prepare environment.
Install-Module Pscx -Scope CurrentUser -AllowClobber -Force
Install-Module VSSetup -Scope CurrentUser -AllowClobber -Force
Import-VisualStudioVars -Architecture x64

$AllProtocols = [System.Net.SecurityProtocolType]'Tls11,Tls12'
[System.Net.ServicePointManager]::SecurityProtocol = $AllProtocols
$ProgressPreference = 'SilentlyContinue'

# Get and prepare needed dependencies.
$qt_version = "5.15.2"
$maria_version = "10.6.5"
$maria_link = "https://mirror.vpsfree.cz/mariadb/mariadb-$maria_version/winx64-packages/mariadb-$maria_version-winx64.zip"
$maria_output = "maria.zip"
$cmake_version = "3.22.1"
$cmake_link = "https://github.com/Kitware/CMake/releases/download/v$cmake_version/cmake-$cmake_version-windows-x86_64.zip"
$cmake_output = "cmake.zip"

Invoke-WebRequest -Uri "$maria_link" -OutFile "$maria_output"
& ".\resources\scripts\7za\7za.exe" x $maria_output

Invoke-WebRequest -Uri "$cmake_link" -OutFile "$cmake_output"
& ".\resources\scripts\7za\7za.exe" x $cmake_output

$cmake_path = "$old_pwd\cmake-$cmake_version-windows-x86_64\bin\cmake.exe"

# Download Qt itself.
$qt_path = "$old_pwd\qt"
pip3 install -U pip
pip3 install -I aqtinstall

if ($qt_version.StartsWith("6")) {
  aqt install-qt -O "$qt_path" windows desktop $qt_version win64_msvc2019_64 -m qtwebengine qtmultimedia qt5compat qtwebchannel qtpositioning
}
else {
  aqt install-qt -O "$qt_path" windows desktop $qt_version win64_msvc2019_64 -m qtwebengine
}

aqt install-src -O "$qt_path" windows desktop $qt_version --archives qtbase

$qt_qmake = "$qt_path\$qt_version\msvc2019_64\bin\qmake.exe"
$env:PATH = "$qt_path\$qt_version\msvc2019_64\bin\;" + $env:PATH

# Download openssl.
aqt install-tool -O "$qt_path" windows desktop tools_openssl_x64 qt.tools.openssl.win_x64
$openssl_base_path = "$qt_path\Tools\OpenSSL\Win_x64"

# Build dependencies.
$maria_path = "$old_pwd\mariadb-$maria_version-winx64"
$qt_sqldrivers_path = "$qt_path\$qt_version\Src\qtbase\src\plugins\sqldrivers"

cd "$qt_sqldrivers_path"

if ($qt_version.StartsWith("6")) {
  & $cmake_path -G Ninja -DCMAKE_BUILD_TYPE="Release" -DMySQL_INCLUDE_DIR="$maria_path\include\mysql" -DMySQL_LIBRARY="$maria_path\lib\libmariadb.lib"
  & $cmake_path --build .
}
else {
  & $qt_qmake -- MYSQL_INCDIR="$maria_path\include\mysql" MYSQL_LIBDIR="$maria_path\lib"
  nmake.exe sub-mysql
}

cd "$old_pwd"

# Build application.
mkdir "rssguard-build"
cd "rssguard-build"
& "$cmake_path" ".." -G Ninja -DCMAKE_BUILD_TYPE="Release" -DREVISION_FROM_GIT=ON -DUSE_WEBENGINE="$webengine" -DFEEDLY_CLIENT_ID="$env:FEEDLY_CLIENT_ID" -DFEEDLY_CLIENT_SECRET="$env:FEEDLY_CLIENT_SECRET" -DGMAIL_CLIENT_ID="$env:GMAIL_CLIENT_ID" -DGMAIL_CLIENT_SECRET="$env:GMAIL_CLIENT_SECRET" -DINOREADER_CLIENT_ID="$env:INOREADER_CLIENT_ID" -DINOREADER_CLIENT_SECRET="$env:INOREADER_CLIENT_SECRET"
& "$cmake_path" --build .
& "$cmake_path" --install . --prefix app

cd "app"
windeployqt.exe --verbose 1 --no-compiler-runtime --no-translations --release rssguard.exe rssguard.dll
cd ".."

# Copy OpenSSL.
Copy-Item -Path "$openssl_base_path\bin\libcrypto*.dll" -Destination ".\app\"
Copy-Item -Path "$openssl_base_path\bin\libssl*.dll" -Destination ".\app\"

# Copy MySQL.
Copy-Item -Path "$maria_path\lib\libmariadb.dll" -Destination ".\app\"
Copy-Item -Path "$qt_sqldrivers_path\plugins\sqldrivers\qsqlmysql.dll" -Destination ".\app\sqldrivers\" -Force

if ($webengine -eq "ON") {
  $packagebase = "rssguard-${git_tag}-${git_revision}-win64"
}
else {
  $packagebase = "rssguard-${git_tag}-${git_revision}-nowebengine-win64"
}

# Create 7zip package.
& "$old_pwd\resources\scripts\7za\7za.exe" a -t7z -mmt -mx9 "$packagebase.7z" ".\app\*"

# Create NSIS installation package.
& "$old_pwd\resources\scripts\nsis\makensis.exe" "/XOutFile $packagebase.exe" ".\NSIS.template.in"

ls
