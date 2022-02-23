from __future__ import print_function
import json
import os
import time
import traceback
import requests
import socket
import cv2
import math
import numpy as np


def get_ip():
    """
    获取本机IP
    :return:
    """
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('10.223.30.51', 53))
        ip = s.getsockname()[0]
    except Exception as e:
        ip = socket.gethostbyname(socket.gethostname())
    finally:
        s.close()
    return ip

report_ip = os.environ.get('CHIEF_IP', '')
if not report_ip:
    report_ip = get_ip()
	

def report_progress(progress):
    """
    向worker上报训练进度
    :return:
    """
    url = "http://%s:%s/v1/worker/report-progress" % (report_ip, 8080)
    try:
        response = requests.post(url, json=json.dumps(progress), proxies={"http": None, "https": None})
    except Exception as e:
        print("send progress info to worker failed!\nprogress_info: %s, \n%s" % (progress, traceback.format_exc()))
        return False, str(e)
    if response.status_code != 200:
        print("send progress info to worker failed!\nprogress_info: %s, \nreason: %s" % (progress, response.reason))
        return False, response.text
    return True, ""


if __name__ == "__main__":
    ## 上报训练进度
    progress = {"step": 0, "type": "train", "loss": 1.5}
    for i in range(3):
        time.sleep(3*60)
        ret, msg = report_progress(progress)
        print("ret: %s, %s" % (ret, msg))
    
    ## 3 上报测试进度
    progress = {"step": 0, "type": "test", "accracy": 0.3}
    for i in range(3):
        time.sleep(3*60)
        ret, msg = report_progress(progress)
        print("ret: %s, %s" % (ret, msg))

def tonemap(matrix, gamma=2.2):
  return np.clip(matrix ** (1.0/gamma), 0, 1)


def calculate_psnr(img1, img2):
    # img1 and img2 have range [0, 255]
    img1 = img1.astype(np.float64)
    img2 = img2.astype(np.float64)
    mse = np.mean((img1 - img2)**2)
    if mse == 0:
        # return float('inf')
        return 0.0
    return 20 * math.log10(255.0 / math.sqrt(mse))


def ssim(img1, img2):
    C1 = (0.01 * 255)**2
    C2 = (0.03 * 255)**2

    img1 = img1.astype(np.float64)
    img2 = img2.astype(np.float64)
    kernel = cv2.getGaussianKernel(11, 1.5)
    window = np.outer(kernel, kernel.transpose())

    mu1 = cv2.filter2D(img1, -1, window)[5:-5, 5:-5]  # valid
    mu2 = cv2.filter2D(img2, -1, window)[5:-5, 5:-5]
    mu1_sq = mu1**2
    mu2_sq = mu2**2
    mu1_mu2 = mu1 * mu2
    sigma1_sq = cv2.filter2D(img1**2, -1, window)[5:-5, 5:-5] - mu1_sq
    sigma2_sq = cv2.filter2D(img2**2, -1, window)[5:-5, 5:-5] - mu2_sq
    sigma12 = cv2.filter2D(img1 * img2, -1, window)[5:-5, 5:-5] - mu1_mu2

    ssim_map = ((2 * mu1_mu2 + C1) * (2 * sigma12 + C2)) / ((mu1_sq + mu2_sq + C1) *
                                                            (sigma1_sq + sigma2_sq + C2))
    return ssim_map.mean()


def calculate_ssim(img1, img2):
    '''calculate SSIM
    the same outputs as MATLAB's
    img1, img2: [0, 255]
    '''
    if not img1.shape == img2.shape:
        raise ValueError('Input images must have the same dimensions.')
    if img1.ndim == 2:
        return ssim(img1, img2)
    elif img1.ndim == 3:
        if img1.shape[2] == 3:
            ssims = []
            for i in range(3):
                ssims.append(ssim(img1, img2))
            return np.array(ssims).mean()
        elif img1.shape[2] == 1:
            return ssim(np.squeeze(img1), np.squeeze(img2))
    else:
        raise ValueError('Wrong input image dimensions.')


def mrse(img1, img2):
    img1 = img1.astype(np.float64)
    img2 = img2.astype(np.float64)
    num = (img1 - img2)**2
    denom = img2**2 + 1.0e-2
    relMse = np.divide(num, denom)
    relMseMean = 0.5*np.mean(relMse)
    return relMseMean

def calculate_mrse(img1, img2):
    '''calculate MRSE
    '''
    if not img1.shape == img2.shape:
        raise ValueError('Input images must have the same dimensions.')
    return mrse(img1, img2)



def save_output_png(input, path, name, mode='clip', cmap=None):
    if not os.path.exists(path):
        os.mkdir(path)
    img = input.detach().squeeze()
    if len(img.size()) == 3:
        img = img.permute(1, 2, 0)
    img = img.cpu().numpy()
    if mode == 'clip':
        img = img * 255.0
        img = np.clip(img, 0, 255)
    elif mode == 'norm':
        img -= np.min(img)
        img /= np.max(img)
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    cv2.imwrite(os.path.join(path, name + '.png'), img)


def save_output_exr(input, path, name, cmap=None):
    if not os.path.exists(path):
        os.mkdir(path)
    img = input.detach().squeeze()
    if len(img.size()) == 3:
        img = img.permute(1, 2, 0)
    img = img.cpu().numpy()
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    cv2.imwrite(os.path.join(path, name + '.exr'), img)