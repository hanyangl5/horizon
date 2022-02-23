#!/bin/sh
# for jizhi platform
cd /jizhi/jizhi2/worker/trainer/debug
CUBLAS_WORKSPACE_CONFIG=:4096:8 python3 train.py
CUBLAS_WORKSPACE_CONFIG=:4096:8 python3 test.py
CUBLAS_WORKSPACE_CONFIG=:4096:8 python3 test.py --checkpoint psnr
CUBLAS_WORKSPACE_CONFIG=:4096:8 python3 test.py --checkpoint ssim
CUBLAS_WORKSPACE_CONFIG=:4096:8 python3 test.py --checkpoint rmse
