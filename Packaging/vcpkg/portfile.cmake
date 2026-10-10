# vcpkg port of Carbon. Carbon is built with its own CMake; FreeType, HarfBuzz and stb come from vcpkg through
# Carbon's dependency switches (CARBON_DEPS_<NAME>_BUILD=OFF). See Docs/Building.md, "vcpkg".

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO eliasertl/Carbon
    REF "v${VERSION}"
    SHA512 1deedaf2f35212ed378c092279cfe4712476a3b04c75180fffe869180add7dee80c6d50887eb408342c2731530263d0860441834eded29b6cc63bdaac3d2bf63
    HEAD_REF main
)

# GitHub's archive of the tag has no submodules. The fonts and icons Carbon embeds are assets, not libraries, so
# they are downloaded from the commits the submodules pin and put where Carbon's build looks for them.
function(carbon_download_asset REPOSITORY COMMIT FILE SHA512 DESTINATION)
    string(REPLACE "[" "%5B" url_file "${FILE}")
    string(REPLACE "]" "%5D" url_file "${url_file}")
    string(MAKE_C_IDENTIFIER "${REPOSITORY}-${FILE}" cache_name)
    string(SUBSTRING "${COMMIT}" 0 10 short_commit)
    get_filename_component(extension "${FILE}" LAST_EXT)
    vcpkg_download_distfile(asset
        URLS "https://raw.githubusercontent.com/${REPOSITORY}/${COMMIT}/${url_file}"
        FILENAME "carbon-${cache_name}-${short_commit}${extension}"
        SHA512 ${SHA512}
    )
    configure_file("${asset}" "${SOURCE_PATH}/ThirdParty/${DESTINATION}/${FILE}" COPYONLY)
endfunction()

set(public_sans_commit c7923167a592d941646f99fb7b5fba17aa7d69e1)     # v2.001
set(jetbrains_mono_commit cd5227bd1f61dff3bbd6c814ceaf7ffd95e947d9)  # v2.304
set(phosphor_commit 70854726d7bd82ae21f0dc81b5b5c35240a77066)
carbon_download_asset(uswds/public-sans ${public_sans_commit} "fonts/variable/PublicSans[wght].ttf"
    4085bce62c227b58832add6980b04754f1060ccecd965ff1a9415e4912e6bf17c5da1e94503ee650c0fbdfcc2a2660f4de714bbe4839ceb88899bc4f64dbe0a9
    PublicSans)
carbon_download_asset(uswds/public-sans ${public_sans_commit} "fonts/variable/PublicSans-Italic[wght].ttf"
    3e68085901a6160c2233c2a6774ebc47cd88a8a788937cac2d52a605ed66914f2dd7ab98c2130dbbf358ed47e8723dcbf2dfad127ae8b74911a62db06f000715
    PublicSans)
carbon_download_asset(uswds/public-sans ${public_sans_commit} "OFL.txt"
    7570e8be5f99e1df7f9486844f98aa93d7d811ac5b713075228559d5b1785248b8ce9bbd4fdfe0fffdf38af9e45beb4ce3a23f6ccc45c5698ac5e5aaf800a9f9
    PublicSans)
carbon_download_asset(JetBrains/JetBrainsMono ${jetbrains_mono_commit} "fonts/variable/JetBrainsMono[wght].ttf"
    5fbc9da5564159e7563d6e50f42fd38e1bd8ae527b66ca29cb01b15bb9a33013c0c37221a657565bf39ef0c4a742c74cfc29d03a66081fa546bf6aa65d70cf51
    JetBrainsMono)
carbon_download_asset(JetBrains/JetBrainsMono ${jetbrains_mono_commit} "fonts/variable/JetBrainsMono-Italic[wght].ttf"
    7f9a1251c69aab08f94142431825cc39a210def00af808bed87813c3c693506b0c4cf6282dbaaa6ebd2014dc8dc0761afd45c47e2a1b06d02747ccea0a86663f
    JetBrainsMono)
carbon_download_asset(JetBrains/JetBrainsMono ${jetbrains_mono_commit} "OFL.txt"
    b5dd9fec3988681a38c57fa6852155b0d701db77923bd39375e7ae17002674eb48377a49a3f360a8d9c3e0723758405bfc046e6f435e86365ddd29f5db7e7cfc
    JetBrainsMono)
carbon_download_asset(phosphor-icons/web ${phosphor_commit} "src/regular/style.css"
    73b14fe9422707df4bdfd7fe031a94f3826d8e48e250188f934cfcd61a050c994790f21da68ca1914d87703e4886b71b54817e030c5a6843b480d55144a8dec8
    Phosphor)
carbon_download_asset(phosphor-icons/web ${phosphor_commit} "src/regular/Phosphor.ttf"
    1737901e93af519a2cd05291e6b57d50adcf5803503c183db873aba54eebf4e2be936b452991f21e70e41c477182f735174ffa1242893581a31f1288e2c3616a
    Phosphor)
carbon_download_asset(phosphor-icons/web ${phosphor_commit} "src/bold/Phosphor-Bold.ttf"
    f64ddd10cae137575f363f538da4735889953bbb907d1494e1c33d961fc9939566b1f81a9345a36807ace6e69dc6b0fa73f590d79e84684266231624eeb03bb6
    Phosphor)
carbon_download_asset(phosphor-icons/web ${phosphor_commit} "src/fill/Phosphor-Fill.ttf"
    c77ba2617a0cd2dbe5a5241f44759cb0aaceb24893a4823d9a5af14c8645fa46b48f390cdd7a34c99a26b31e6f523e3e291e1ebf8a9010c7bbeb35a283ec1470
    Phosphor)
carbon_download_asset(phosphor-icons/web ${phosphor_commit} "LICENSE"
    8fa4f26ecc035475b0753d8b564307782daa3574b60e8a4b053721dfbf3fccb8fae84f28cc9e4fc104e7c430abf247d8886b700a8768574740c8f1841655efd2
    Phosphor)

# Every backend is set explicitly, so that what is built does not depend on what is installed on the machine.
vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        opengl   CARBON_BACKEND_OPENGL
        opengles CARBON_BACKEND_OPENGLES
        vulkan   CARBON_BACKEND_VULKAN
        dx11     CARBON_BACKEND_DX11
        dx9      CARBON_BACKEND_DX9
        webgpu   CARBON_BACKEND_WEBGPU
)
if("vulkan" IN_LIST FEATURES)
    list(APPEND FEATURE_OPTIONS
        "-DVulkan_GLSLC_EXECUTABLE=${CURRENT_HOST_INSTALLED_DIR}/tools/shaderc/glslc${VCPKG_HOST_EXECUTABLE_SUFFIX}")
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DCARBON_BUILD_EXTENSIONS=ON
        -DCARBON_BUILD_REFLECTION=ON
        -DCARBON_BUILD_EXAMPLES=OFF
        -DCARBON_BUILD_TESTS=OFF
        -DCARBON_BUILD_BENCHMARKS=OFF
        -DCARBON_INSTALL=ON
        -DCARBON_DEPS_FREETYPE_BUILD=OFF
        -DCARBON_DEPS_HARFBUZZ_BUILD=OFF
        -DCARBON_DEPS_STB_BUILD=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/Carbon)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
# Carbon installs its license and the licenses of the embedded fonts into share/doc/Carbon; vcpkg wants them in
# share/carbon.
file(GLOB font_licenses "${CURRENT_PACKAGES_DIR}/share/doc/Carbon/Licenses/*")
file(COPY ${font_licenses} DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}/Licenses")
file(COPY "${CURRENT_PACKAGES_DIR}/share/doc/Carbon/THIRD_PARTY_NOTICES.md" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/share/doc")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST
    "${SOURCE_PATH}/LICENSE"
    "${SOURCE_PATH}/ThirdParty/PublicSans/OFL.txt"
    "${SOURCE_PATH}/ThirdParty/JetBrainsMono/OFL.txt"
    "${SOURCE_PATH}/ThirdParty/Phosphor/LICENSE"
)
