licenses(["notice"])

package(default_visibility = ["//visibility:public"])

load("@rules_proto//proto:defs.bzl", "proto_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@com_github_grpc_grpc//bazel:grpc_build_system.bzl", "grpc_proto_library")

# The following three rules demonstrate the usage of the cc_grpc_library rule in
# in a mode compatible with the native proto_library and cc_proto_library rules.

proto_library(
    name = "kvstore_proto",
    srcs = ["kvstore.proto"],
)

cc_proto_library(
    name = "kvstore_cc_proto",
    deps = [":kvstore_proto"],
)

cc_grpc_library(
    name = "kvstore_cc_grpc",
    srcs = [":kvstore_proto"],
    grpc_only = True,
    deps = [":kvstore_cc_proto"],
)
