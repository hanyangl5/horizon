#!/usr/bin/env python
# -*- coding:utf-8 -*-

import os, sys


def convert(rootdir, optype='l'):
    """
    目录/文件名转换成小写或大写
    :param rootdir: 要转换的根目录路径
    :param optype: 操作类型（小写/大写）  小写：optype = 'l'  大写：optype = 'u'，默认转换成小写
    :return:
    """
    dirlist = os.listdir(rootdir)
    for index, onedir in enumerate(dirlist):
        if optype == 'l':
            cdir = onedir.lower()
        elif optype == 'u':
            cdir = onedir.upper()
        subdir = os.path.join(rootdir, onedir)

        if onedir != cdir:      # 子目录或文件名不全为小写或大写
            subcdir = os.path.join(rootdir, cdir)
            os.rename(subdir, subcdir)          # 把大写的目录或文件名命名成小写或大写
            dirlist[index] = cdir
            subdir = subcdir

        # print(subdir)
        if os.path.isdir(subdir):
            convert(subdir, optype)

def help():
    print("use: python convert.py 'rootDirPath' {optype:'l'/'u'}")

def main():
    if len(sys.argv) == 2:
        convert(sys.argv[1])
    elif len(sys.argv) == 3:
        convert("interface", sys.argv[2])
    else:
        help()
        exit(400)


if __name__ == '__main__':
    main()