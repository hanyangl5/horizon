import logging
import torch

save_model_dir = '/apdcephfs/share_1330077/ulyseszhang/save3/rae_diningroom_1_19'
train_root_dir = '/apdcephfs/share_1330077/mingyanzhu/datasets/CloudAI/spp32768_train'
#train_root_dir = '/apdcephfs/share_1330077/ulyseszhang/dataset/spp32768_train'
val_root_dir   = '/apdcephfs/share_1330077/mingyanzhu/datasets/CloudAI/spp32768_val'
test_image_dir = '/apdcephfs/share_1330077/mingyanzhu/datasets/CloudAI/spp32768_test'
#test_image_dir = '/apdcephfs/share_1330077/ulyseszhang/spp1024_test'
save_image_dir = '/apdcephfs/share_1330077/ulyseszhang/save3/rae_diningroom_1_19'


model_name = 'RAE'

recursion_step = 7
epoch_num = 200

# ------- param --------------
dilation = False
deform = False
# -----------------------------

train_width = 256
test_width = 512
batch_size = 6
num_workers = 6
best_checkpoint = True

resume = False
resume_model_path = ''
scale = 1

train_scenes = ['diningroom'] # BistroInterior, sponza, BistroExterior
test_scenes = ['diningroom']
# spp0.25 is named as images
#train_default_size = [1024, 2048]
#test_default_size = [1024, 2048]
#train_spp_dir = 'images'
#test_spp_dir = 'images'
train_default_size = [720, 1280]
test_default_size = [720, 1280]
train_spp_dir = 'images_bounce5'
test_spp_dir = 'images_bounce5'
cache = True
data_repeat = 10
multi_gpu = False
#multi_gpu = torch.cuda.device_count() > 1
loss_weight = [0.05, 0.25, 0.5, 0.75, 1]

manual_random_seed = 42

# -----------logger--------------
logger = logging.getLogger('train')
# logger.setLevel(logging.INFO)
logger.setLevel(logging.DEBUG)

ch = logging.StreamHandler()
# ch.setLevel(logging.INFO)
ch.setLevel(logging.DEBUG)

formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
ch.setFormatter(formatter)
logger.addHandler(ch)

# ----------- Val logger--------------
val_logger = logging.getLogger('val')
val_logger.setLevel(logging.DEBUG)