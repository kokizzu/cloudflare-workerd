# WARNING: THIS FILE IS AUTOGENERATED BY update-deps.py DO NOT EDIT

load("@//:build/http.bzl", "http_file")

TAG_NAME = "0.56.0"
URL = "https://github.com/bazelbuild/rules_rust/releases/download/0.56.0/cargo-bazel-x86_64-pc-windows-msvc.exe"
SHA256 = "5c62a1c42b71af6c2765dc5f9518eba22ed28276a65f7accf5a2bca0c5cfaf74"

def dep_cargo_bazel_win_x64():
    http_file(
        name = "cargo_bazel_win_x64",
        url = URL,
        executable = True,
        sha256 = SHA256,
        downloaded_file_path = "downloaded.exe",
    )
