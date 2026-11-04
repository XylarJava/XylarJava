cmake_minimum_required(VERSION 3.15)
project(launcher Java)
find_package(Java 1.8 REQUIRED COMPONENTS Development)

include(UseJava)
set(CMAKE_JAVA_JAR_ENTRY_POINT io.xylarjava.newlaunch.EntryPoint)
set(CMAKE_JAVA_COMPILE_FLAGS -target 8 -source 8)

set(SRC
    io/xylarjava/newlaunch/EntryPoint.java
    io/xylarjava/newlaunch/launcher/Launcher.java
    io/xylarjava/newlaunch/launcher/impl/AbstractLauncher.java
    io/xylarjava/newlaunch/launcher/impl/StandardLauncher.java
    io/xylarjava/newlaunch/exception/ParameterNotFoundException.java
    io/xylarjava/newlaunch/exception/ParseException.java
    io/xylarjava/newlaunch/utils/Parameters.java
    io/xylarjava/newlaunch/utils/ReflectionUtils.java
    io/xylarjava/newlaunch/utils/logging/Level.java
    io/xylarjava/newlaunch/utils/logging/Log.java
    io/xylarjava/newlaunch/legacy/LegacyProxy.java
)

set(LEGACY_SRC
    legacy/io/xylarjava/newlaunch/legacy/LegacyFrame.java
    legacy/io/xylarjava/newlaunch/legacy/LegacyLauncher.java
    legacy/io/xylarjava/newlaunch/legacy/fix/online/Handler.java
    legacy/io/xylarjava/newlaunch/legacy/fix/online/OnlineFixes.java
    legacy/io/xylarjava/newlaunch/legacy/fix/online/OnlineModeFix.java
    legacy/io/xylarjava/newlaunch/legacy/fix/online/SkinFix.java
    legacy/io/xylarjava/newlaunch/legacy/utils/Base64.java
    legacy/io/xylarjava/newlaunch/legacy/utils/api/MojangApi.java
    legacy/io/xylarjava/newlaunch/legacy/utils/api/Texture.java
    legacy/io/xylarjava/newlaunch/legacy/utils/json/JsonParseException.java
    legacy/io/xylarjava/newlaunch/legacy/utils/json/JsonParser.java
    legacy/io/xylarjava/newlaunch/legacy/utils/url/ByteArrayUrlConnection.java
    legacy/io/xylarjava/newlaunch/legacy/utils/url/UrlUtils.java
    legacy/net/minecraft/Launcher.java
    legacy/io/xylarjava/newlaunch/legacy/LegacyProxy.java
)

add_jar(NewLaunch ${SRC})
add_jar(NewLaunchLegacy ${LEGACY_SRC} INCLUDE_JARS NewLaunch)
install_jar(NewLaunch "${JARS_DEST_DIR}")
install_jar(NewLaunchLegacy "${JARS_DEST_DIR}")
