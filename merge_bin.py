Import("env")

import subprocess
import os

def merge_bins(source, target, env):
    build_dir   = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    board       = env.subst("$BOARD")
    esptool     = env.subst("$OBJCOPY")

    output = os.path.join(project_dir, f"kprox_{board}_full.bin")

    cmd = [
        esptool, "--chip", "esp32s3", "merge_bin",
        "-o", output,
        "--flash_mode", "dio",
        "--flash_freq", "80m",
        "--flash_size", "8MB",
        "0x0000",   os.path.join(build_dir, "bootloader.bin"),
        "0x8000",   os.path.join(build_dir, "partitions.bin"),
        "0x10000",  os.path.join(build_dir, "firmware.bin"),
        "0x610000", os.path.join(build_dir, "spiffs.bin"),
    ]

    print(f"Merging firmware -> {output}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr)
    else:
        print("Merge complete.")

env.AddPostAction("$BUILD_DIR/firmware.bin", merge_bins)
