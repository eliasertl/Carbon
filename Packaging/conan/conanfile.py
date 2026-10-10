import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, download, get, rmdir

required_conan_version = ">=2.0"


class CarbonConan(ConanFile):
    """Conan 2 recipe of Carbon, built with Carbon's own CMake. FreeType, HarfBuzz and stb come from Conan through
    Carbon's dependency switches (CARBON_DEPS_<NAME>_BUILD=OFF). See Docs/Building.md, "Conan"."""

    name = "carbon"
    version = "1.0.0"
    description = "Immediate-mode C++20 UI framework with a macOS look"
    license = ("MIT", "OFL-1.1")
    url = "https://github.com/eliasertl/Carbon"
    homepage = "https://github.com/eliasertl/Carbon"
    topics = ("gui", "ui", "immediate-mode", "widgets", "webgpu", "vulkan", "opengl", "direct3d")
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "fPIC": [True, False],
        "with_opengl": [True, False],
        "with_opengles": [True, False],
        "with_vulkan": [True, False],
        "with_dx11": [True, False],
        "with_dx9": [True, False],
        "with_webgpu": [True, False],
    }
    default_options = {
        "fPIC": True,
        "with_opengl": True,
        "with_opengles": True,
        "with_vulkan": False,
        "with_dx11": False,
        "with_dx9": False,
        "with_webgpu": False,
        # Carbon shapes with HarfBuzz's own Unicode functions; GLib would only add a large dependency tree.
        "harfbuzz/*:with_glib": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        else:
            del self.options.with_dx11
            del self.options.with_dx9

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        self.requires("freetype/2.14.3")
        self.requires("harfbuzz/12.3.0")
        self.requires("stb/cci.20240531")
        if self.options.with_vulkan:
            self.requires("vulkan-loader/1.4.357.0")
            self.requires("vulkan-headers/1.4.357.0")
        if self.options.with_webgpu:
            raise ConanInvalidConfiguration(
                "ConanCenter has no Dawn package. Build Carbon with Dawn from its own CMake (Docs/Building.md).")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.25 <5]")
        if self.options.with_vulkan:
            self.tool_requires("shaderc/2026.4")

    def validate(self):
        check_min_cppstd(self, 20)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        # GitHub's archive of the tag has no submodules. The fonts and icons Carbon embeds are assets, not
        # libraries, so they are downloaded from the commits the submodules pin and put where Carbon's build looks.
        for asset in self.conan_data["assets"][self.version]:
            destination = os.path.join(self.source_folder, "ThirdParty", asset["destination"])
            os.makedirs(os.path.dirname(destination), exist_ok=True)
            download(self, asset["url"], destination, sha256=asset["sha256"])

    def generate(self):
        toolchain = CMakeToolchain(self)
        variables = toolchain.cache_variables
        variables["CARBON_BUILD_EXTENSIONS"] = True
        variables["CARBON_BUILD_REFLECTION"] = True
        variables["CARBON_BUILD_EXAMPLES"] = False
        variables["CARBON_BUILD_TESTS"] = False
        variables["CARBON_BUILD_BENCHMARKS"] = False
        variables["CARBON_INSTALL"] = True
        variables["CARBON_DEPS_FREETYPE_BUILD"] = False
        variables["CARBON_DEPS_HARFBUZZ_BUILD"] = False
        variables["CARBON_DEPS_STB_BUILD"] = False
        # Every backend is set explicitly, so that what is built does not depend on what is installed.
        variables["CARBON_BACKEND_OPENGL"] = bool(self.options.with_opengl)
        variables["CARBON_BACKEND_OPENGLES"] = bool(self.options.with_opengles)
        variables["CARBON_BACKEND_VULKAN"] = bool(self.options.with_vulkan)
        variables["CARBON_BACKEND_DX11"] = bool(self.options.get_safe("with_dx11", False))
        variables["CARBON_BACKEND_DX9"] = bool(self.options.get_safe("with_dx9", False))
        variables["CARBON_BACKEND_WEBGPU"] = False
        if self.options.with_vulkan:
            # glslc compiles the Vulkan backend's shaders at build time; it comes from the shaderc tool package.
            shaderc = self.dependencies.build["shaderc"]
            executable = "glslc.exe" if self.settings_build.os == "Windows" else "glslc"
            variables["Vulkan_GLSLC_EXECUTABLE"] = os.path.join(shaderc.cpp_info.bindirs[0], executable).replace(
                "\\", "/")
        toolchain.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        licenses = os.path.join(self.package_folder, "licenses")
        copy(self, "LICENSE", self.source_folder, licenses)
        copy(self, "*", os.path.join(self.package_folder, "share", "doc", "Carbon"), licenses)
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        # The package's own CarbonConfig.cmake is used: it finds FreeType and HarfBuzz again (from Conan's CMakeDeps
        # files), defines Carbon::Carbon, Carbon::Extensions and Carbon::Reflection, Carbon_BACKENDS and
        # carbon_copy_dawn_runtime, exactly as an installed Carbon does without Conan.
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = ["."]
        self.cpp_info.libs = ["CarbonReflection", "CarbonExtensions", "Carbon"]
