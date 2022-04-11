load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

http_archive(
    name = "com_github_grpc_grpc",
    urls = [
        "https://github.com/grpc/grpc/archive/11f00485aa5ad422cfe2d9d90589158f46954101.tar.gz",
    ],
    strip_prefix = "grpc-11f00485aa5ad422cfe2d9d90589158f46954101",
)
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()
load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
grpc_extra_deps()

http_archive(
    name = "rules_cc",
    urls = [
        "https://github.com/bazelbuild/rules_cc/archive/aa7ff810cf5ec99ca34f784743b33631b74c2d16",
    ],
)

http_archive(
    name = "com_github_tencent_rapidjson",
    urls = [
        "https://github.com/Tencent/rapidjson/archive/f54b0e47a08782a6131cc3d60f94d038fa6e0a51.tar.gz",
    ],
    strip_prefix = "rapidjson-f54b0e47a08782a6131cc3d60f94d038fa6e0a51",
    build_file = "//bazel/rapidjson:BUILD",
    sha256 = "4a76453d36770c9628d7d175a2e9baccbfbd2169ced44f0cb72e86c5f5f2f7cd",
)

git_repository(
    name = "com_github_nelhage_rules_boost",
    commit = "fce83babe3f6287bccb45d2df013a309fa3194b8",
    remote = "https://github.com/nelhage/rules_boost",
    shallow_since = "1591047380 -0700",
)

load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
boost_deps()
