from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain


class AetherisIuiConan(ConanFile):
    name = "aetheris-iui"
    version = "0.1.0"
    package_type = "library"

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tests": [True, False],
        "with_property_tests": [True, False],
        "with_benchmarks": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tests": True,
        "with_property_tests": True,
        "with_benchmarks": False,
    }

    exports_sources = (
        "CMakeLists.txt",
        "CMakePresets.json",
        "application/*",
        "bench/*",
        "cmake/*",
        "domain/*",
        "infrastructure/*",
        "interface/*",
        "tests/*",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def requirements(self):
        self.requires("nlohmann_json/3.11.3")
        if self.options.with_tests:
            self.test_requires("gtest/1.17.0")
        if self.options.with_property_tests:
            self.test_requires("rapidcheck/cci.20231215")
        if self.options.with_benchmarks:
            self.test_requires("benchmark/1.9.5")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        toolchain = CMakeToolchain(self)
        toolchain.variables["AETHERIS_ENABLE_TESTS"] = bool(self.options.with_tests)
        toolchain.variables["AETHERIS_ENABLE_BENCHMARKS"] = bool(self.options.with_benchmarks)
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
