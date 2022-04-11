licenses(["notice"])

cc_binary(
    name = "http_server",
    srcs = ["front_end_server.cc",
            "common_mail_files.h",
            "common_mail_files.cc",
            "mail.h",
            "mail.cc",
            "files.h",
            "files.cc",
            "front_end_kv_client.h",
            "front_end_kv_client.cc",
            "http.h",
            "http.cc",
            "users.h",
            "users.cc",
            "admin.h",
            "admin.cc",
            "smtp.h",
            "smtp_functions.cc",
            "pop3.h",
            "pop3_functions.cc"
            ],
    linkopts = ["-lresolv"],
    defines = ["BAZEL_BUILD"],
    deps = [
        "@boost//:uuid",
        "@boost//:functional",
        "@com_github_tencent_rapidjson//:rapidjson",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_reflection",
        "//kvstore/protos:kvstore_cc_grpc",
    ]
)

cc_binary(
    name = "smtp_server",
    srcs = ["smtp.cc",
            "smtp.h",
            "smtp_functions.cc",
            "mail.h",
            "mail.cc",
            "common_mail_files.h",
            "common_mail_files.cc",
            "front_end_kv_client.h",
            "front_end_kv_client.cc"
            ],
    linkopts = ["-lresolv"],
    defines = ["BAZEL_BUILD"],
    deps = [
        "@boost//:uuid",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_reflection",
        "//kvstore/protos:kvstore_cc_grpc",
    ]
)


cc_binary(
    name = "pop3_server",
    srcs = ["pop3.cc",
            "pop3.h",
            "pop3_functions.cc",
            "mail.h",
            "mail.cc",
            "common_mail_files.h",
            "common_mail_files.cc",
            "front_end_kv_client.h",
            "front_end_kv_client.cc"
            ],
    linkopts = ["-lresolv"],
    defines = ["BAZEL_BUILD"],
    deps = [
        "@boost//:uuid",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_reflection",
        "//kvstore/protos:kvstore_cc_grpc",
    ]
)
