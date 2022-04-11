licenses(["notice"])

cc_binary(
    name = "master",
    srcs = ["master.cpp",
            "kv_cache.cpp", "kv_cache.hh",
            "utils.cpp", "utils.hh"],
    defines = ["BAZEL_BUILD"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_reflection",
        "//kvstore/protos:kvstore_cc_grpc"
    ]
)

cc_binary(
    name = "kv_store",
    srcs = ["kv_store.cpp", "kv_store.hh",
            "kv_cache.cpp", "kv_cache.hh",
            "utils.cpp", "utils.hh"],
    defines = ["BAZEL_BUILD"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_reflection",
        "//kvstore/protos:kvstore_cc_grpc"
    ]
)

cc_binary(
    name = "kv_client",
    srcs = ["kv_client.cpp",
            "kv_cache.cpp", "kv_cache.hh",
            "utils.cpp", "utils.hh"],
    defines = ["BAZEL_BUILD"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_reflection",
        "//kvstore/protos:kvstore_cc_grpc"
    ]
)
