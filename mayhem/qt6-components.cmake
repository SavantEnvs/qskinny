# Injected via CMAKE_PROJECT_INCLUDE by mayhem/build.sh (no upstream file is modified):
# make the Qt6 components QSkinny links against available, including the versionless Qt:: targets.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick OpenGL Svg)
