"""
//---------------------------------------------------------------------
// GBuffer:
//          | R             | G         | B         | A               |
// Color    | albedo                                                  |
// Normal   | normal                                | AlphaMode       |
// Position | position                              | HitModelFlag    |
// Emissive | emissive                              | AO              |
// PBR      | bDoubleSided  | roughness | metallic  | AlphaCutoff     |
// FWidth   | N Width       | depth     | position  | PrimitiveID     |
//          | R16           | G16       |           |                 |
// Velocity | x             | y         | ViewDist  | Mesh ID         |
// NDC      | x             | y         | z         | w               |
//---------------------------------------------------------------------
"""
import os
import torch
import pyexr
import numpy as np
from multiprocessing import Pool
from torch.utils.data import Dataset

import settings
logger = settings.logger

def crop_img(image, start, size):
    y_start = start[0]
    x_start = start[1]
    y_end = y_start + size
    x_end = x_start + size

    return image[y_start: y_end, x_start: x_end, :]

def pyexr_read_image(path):
    file = pyexr.open(path)
    if 'HDRImg.exr' in path or 'GBufferColor.exr' in path or 'GBufferNormal.exr' in path:
        return path, file.get()[..., :3]
    elif 'GBufferPBR.exr' in path:
        return path, file.get()[..., 1:3]
    else:
        return path, file.get()


def normalization(image, albedo):
    image /= albedo
    image = np.log1p(image)
    return image


class ExrDataset(Dataset):
    def __init__(self, root_dir, is_train=True):
        self.root_dir = root_dir
        self.is_train = is_train
        self.clip_length = settings.recursion_step if is_train else 1
        self.crop_size = settings.train_width
        self.data_repeat = settings.data_repeat if is_train else 1

        self.clips = []
        self.cache = settings.cache
        self.in_memory_cache = {} if self.cache else None
        self.spp_dir = settings.train_spp_dir if is_train else settings.test_spp_dir
        self.scene_list = settings.train_scenes if is_train else settings.test_scenes
        self.default_size = settings.train_default_size if is_train else settings.test_default_size

        if is_train:
            self.width = self.height = self.crop_size
        else:
            self.width = self.default_size[1]
            self.height = self.default_size[0]

        total_frames = 0

        for base_scene in self.scene_list:
            base_scene_dir = os.path.join(root_dir, base_scene, self.spp_dir)
            for sub_scene in os.listdir(base_scene_dir):
                sub_scene_dir = os.path.join(base_scene_dir, sub_scene)
                part_total_frames = len(os.listdir(sub_scene_dir))
                if self.is_train and self.clip_length > 1:
                    self.clips += [[base_scene, sub_scene, v] for v in
                                   range(0, part_total_frames - self.clip_length, self.clip_length // 2)]
                else:
                    self.clips += [[base_scene, sub_scene, v] for v in range(0, part_total_frames, self.clip_length)]

                if self.cache:
                    self._cache_in_memory(part_total_frames, os.path.join(root_dir, base_scene, '{}', sub_scene))

                total_frames += part_total_frames

        if self.in_memory_cache is not None:
            print(len(self.in_memory_cache.keys()))
        logger.info('Total {} clips: {:d}, total frames: {:d}'.format('train' if is_train else 'test', len(self.clips), total_frames))

    def _cache_in_memory(self, total_images_num, path):
        print('>>>> Start Cache {} , there are {:d} frames... <<<<'.format(path, total_images_num))
        pool = Pool(25)
        temp_res = None
        temp_keys = []

        for i in range(1, total_images_num + 1):
            temp_lr_dir = os.path.join(path.format(self.spp_dir), '{:04d}'.format(i))
            temp_gt_dir = os.path.join(path.format('ground_truth'), '{:04d}'.format(i))

            # 'GBufferFWidth.exr', 'GBufferTransparent.exr', 'GBufferShadow.exr'
            for file_name in ['HDRImg.exr', 'GBufferColor.exr', 'GBufferVelocity.exr', 'GBufferNormal.exr',
                              'GBufferPBR.exr']:
                lr_image_file = os.path.join(temp_lr_dir, file_name)
                temp_keys.append(lr_image_file)

            for file_name in ['HDRImg.exr', 'GBufferColor.exr']:
                gt_image_file = os.path.join(temp_gt_dir, file_name)
                temp_keys.append(gt_image_file)

        temp_res = pool.map(pyexr_read_image, temp_keys)
        for value in temp_res:
            self.in_memory_cache[value[0]] = value[1]

        print("Total cache {:d} images.".format(len(self.in_memory_cache)))
        pool.close()
        pool.join()

    def read(self, path, start):
        if self.cache:
            data = self.in_memory_cache[path]
        else:
            _, data = pyexr_read_image(path)
        if start is not None:
            data = crop_img(data, start, self.crop_size)
        data = np.expand_dims(data, 0)
        return data

    def set_start(self):
        y_start = np.random.randint(self.default_size[0] - self.crop_size)
        x_start = np.random.randint(self.default_size[1] - self.crop_size)
        return [y_start, x_start]

    def __getitem__(self, idx):
        base_scene, sub_scene, start_frame = self.clips[idx % len(self.clips)]
        root = os.path.join(self.root_dir, base_scene, '{}', sub_scene)

        A = np.zeros((self.clip_length, self.height, self.width, 8), dtype=np.float)
        B = np.zeros((self.clip_length, self.height, self.width, 3), dtype=np.float)
        ALBEDO = np.zeros((self.clip_length, self.height, self.width, 6), dtype=np.float)

        depths = []
        albedos = []
        mvs = []
        gts = []
        gt_albedos = []
        images = []
        normals = []
        roughnesses = []
        start = self.set_start() if self.is_train else None

        for frame in range(start_frame, start_frame + self.clip_length):
            file_num = str(frame + 1).zfill(4)

            # Ground Truth Image
            gt_image = self.read(os.path.join(root.format('ground_truth'), file_num, 'HDRImg.exr'), start)
            gts.append(gt_image)

            # Ground Truth Albedo
            gt_albedo = self.read(os.path.join(root.format('ground_truth'), file_num, 'GBufferColor.exr'), start)
            gt_albedos.append(gt_albedo)

            # Images Image
            images_image = self.read(os.path.join(root.format(self.spp_dir), file_num, 'HDRImg.exr'), start)
            images.append(images_image)

            # Images Albedo
            images_albedo = self.read(os.path.join(root.format(self.spp_dir), file_num, 'GBufferColor.exr'), start)
            albedos.append(images_albedo)

            # Images Velocity
            images_velocity = self.read(os.path.join(root.format(self.spp_dir), file_num, 'GBufferVelocity.exr'), start)
            depths.append(images_velocity[..., 2:3])

            # Images Normal
            images_normal = self.read(os.path.join(root.format(self.spp_dir), file_num, 'GBufferNormal.exr'), start)
            normals.append(images_normal)

            # Images PBR
            images_pbr = self.read(os.path.join(root.format(self.spp_dir), file_num, 'GBufferPBR.exr'), start)
            roughness =  images_pbr[..., 1:2]
            roughnesses.append(roughness)
            

        gts = np.concatenate(gts, axis=0)
        gt_albedos = np.concatenate(gt_albedos, axis=0)
        images = np.concatenate(images, axis=0)
        albedos = np.concatenate(albedos, axis=0)
        normals = np.concatenate(normals, axis=0)
        depths = np.concatenate(depths, axis=0)
        roughnesses=np.concatenate(roughnesses,axis=0)

        albedos += 0.00316
        images = normalization(images, albedos)
        gt_albedos += 0.00316
        gts = normalization(gts, gt_albedos)

        if abs(np.max(depths)) > 1e-6:
            depths = depths / np.max(depths)

        A[:, :, :, :3] = images
        A[:, :, :, 3:6] = normals
        A[:, :, :, 6:7] = depths
        A[:, :, :, 7:8] = roughnesses

        B[:, :, :, :] = gts
        ALBEDO[:, :, :, 0:3] = albedos
        ALBEDO[:, :, :, 3:6] = gt_albedos

        A = torch.from_numpy(A)
        B = torch.from_numpy(B)
        ALBEDO = torch.from_numpy(ALBEDO)

        A = A.permute((0, 3, 1, 2))
        B = B.permute((0, 3, 1, 2))
        ALBEDO = ALBEDO.permute((0, 3, 1, 2))

        return {
            'A': A,
            'B': B,
            'ALBEDO': ALBEDO
        }


    def __len__(self):
        return len(self.clips) * self.data_repeat
