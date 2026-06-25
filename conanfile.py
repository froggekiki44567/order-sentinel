from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout

class OrderSentinelConan(ConanFile):
    name = "order_sentinel"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("boost/1.84.0")
        self.requires("openssl/3.2.1")
        self.requires("nlohmann_json/3.11.3")
        self.requires("catch2/3.5.3")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def layout(self):
        cmake_layout(self)