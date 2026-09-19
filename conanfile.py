from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class WorldChunksDependencies(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        self.requires("expected-lite/0.9.0")
        self.requires("funchook/1.1.3")
        if self.settings.os != "Windows":
            self.requires("openssl/3.6.4")
        self.requires("nlohmann_json/3.12.0")

    def generate(self):
        CMakeDeps(self).generate()
        CMakeToolchain(self).generate()
