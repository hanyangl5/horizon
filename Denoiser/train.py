from torch.utils.data import Dataset, DataLoader
import logging
import torch
import numpy as np
import random
from test import load_checkpoints
from model import *
from dataset import *
from losses import *
from utils import *
from utils import report_progress, calculate_mrse, calculate_ssim, calculate_psnr, save_output_exr, tonemap
from distutils.dir_util import copy_tree

logger = settings.logger
val_logger = settings.val_logger

np.random.seed(settings.manual_random_seed)
torch.manual_seed(settings.manual_random_seed)
torch.cuda.manual_seed(settings.manual_random_seed)
torch.cuda.manual_seed_all(settings.manual_random_seed)
torch.backends.cudnn.benchmark = False
torch.backends.cudnn.deterministic = True
torch.use_deterministic_algorithms(True)
g = torch.Generator()
g.manual_seed(settings.manual_random_seed)


def save_model(epoch, network, optimizer, checkpoint_root_dir, save_model_path=None):
	if save_model_path is None:
		save_model_path = os.path.join(checkpoint_root_dir, settings.model_name + '_' + "{:04d}".format(epoch) + '.pth')
	logger.info('SAVING MODEL AT EPOCH %s' % (epoch + 1))
	torch.save({
		'epoch': epoch,
		'state_dict': network.state_dict(),
		'optimizer': optimizer.state_dict(),
	}, save_model_path)

def train_sequence(model, sequence):
	output_final = sequence['B'].clone()
	output_final.fill_(0)
	target_final = sequence['B'].clone()
	target_final.fill_(0)
	weight_loss = [0.011, 0.044, 0.135, 0.325, 0.607, 0.882, 1]
	inp = sequence['A']
	target = sequence['B']

	loss_final = 0
	ls_final = 0
	lg_final = 0
	lt_final = 0

	for j in range(0, settings.recursion_step):
		inpi = inp[:, j, :, :, :]
		gti = target[:, j, :, :, :]

		final_inp = {
			'A': inpi,
			'B': gti
		}
		#print(inpi.shape)
		model.set_input(final_inp)
		if j == 0:
			model.reset_hidden()

		output = model()
		output_final[:, j, :, :, :] = output
		target_final[:, j, :, :, :] = gti

	output_final = torch.pow(output_final, 0.2)
	target_final = torch.pow(target_final, 0.2)

	temporal_output, temporal_target = get_temporal_data(output_final, target_final)


	for j in range(0, settings.recursion_step):
		output = output_final[:, j, :, :, :]
		target = target_final[:, j, :, :, :]
		t_output = temporal_output[:, j, :, :, :]
		t_target = temporal_target[:, j, :, :, :]

		l, ls, lg, lt = loss_func(output, t_output, target, t_target)
		loss_final += l * weight_loss[j]
		ls_final += ls
		lg_final += lg
		lt_final += lt

	return loss_final, ls_final, lg_final, lt_final


def train(cur_epoch, network, optimizer, training_generator):
	total_loss = 0
	total_ls = 0
	total_lg = 0
	total_lt = 0

	total_loss_num = 0
	start_time = time.time()

	for i, item in enumerate(training_generator):
		for k in item.keys():
			item[k] = item[k].float().cuda()

		optimizer.zero_grad()
		loss_final, ls_final, lg_final, lt_final = train_sequence(network, item)
		loss_final.backward(retain_graph=False)
		optimizer.step()

		total_loss += loss_final.item()
		total_ls += ls_final.item()
		total_lg += lg_final.item()
		total_lt += lt_final.item()
		total_loss_num += 1

		#logger.debug('[TRAIN] epoch: %d  [%d / %d], average tot_loss: %f, L1 spatial loss: %f, L1 temporal loss: %f '
        #                 'L1 HFEN loss: %f' % (cur_epoch + 1, i + 1, len(training_generator), total_loss / (i + 1), ls_final.item(),
        #                 lt_final.item(), lg_final.item()))

	total_loss /= total_loss_num
	logger.info('[TRAIN] epoch: %d  tot_loss: %f time: %f' % (cur_epoch + 1, total_loss, time.time() - start_time))

	return total_loss

def val(cur_epoch, network, testing_generator, save_root_dir):
	fh = logging.FileHandler(os.path.join(save_root_dir, 'validation.log'), mode='w')
	fh.setLevel(logging.DEBUG)
	val_logger.addHandler(fh)
	network.eval()
	psnr = ssim = rmse = 0.
	output_count = 0
	total_time = 0
	with torch.no_grad():
		for i, data in enumerate(testing_generator):
			for k in data.keys():
				data[k] = data[k].float().cuda()

			inp = data['A']
			target = data['B']
			albedo = data['ALBEDO'][:, 0, 0:3, ...]
			gt_albedo = data['ALBEDO'][:, 0, 3:6, ...]

			inpi = inp[:, 0, :, :, :]
			gti = target[:, 0, :, :, :]

			final_inp = {
				'A': inpi,
				'B': gti
			}

			network.set_input(final_inp)
			if i == 0:
				network.reset_hidden()

			start_time = time.time()
			output = network()
			temp_time = time.time() - start_time
			total_time += temp_time

			output = torch.expm1(output)
			output = output * albedo
			final_gt = torch.expm1(gti)
			final_gt = gti * gt_albedo
			save_output_exr(output, save_root_dir, '{:04d}'.format(i+1))

			out = output.cpu().detach().numpy()
			out = np.transpose(out, (0, 2, 3, 1))
			gt = final_gt.cpu().detach().numpy()
			gt = np.transpose(gt, (0, 2, 3, 1))

			temp_psnr = 0
			temp_ssim = 0
			temp_rmse = 0
			temp_count = 0

			for bn in range(gt.shape[0]):
				out_mapping = tonemap(out[bn]) * 255.
				gt_mapping = tonemap(gt[bn]) * 255.
				temp_psnr += calculate_psnr(out_mapping, gt_mapping)
				temp_ssim += calculate_ssim(out_mapping, gt_mapping)
				temp_rmse += calculate_mrse(out[bn], gt[bn])
				temp_count += 1
			val_logger.debug("[Validation {:04d}], count {:d}, psnr: {:.4f}, ssim: {:.4f}, rmse: {:.4f}, time: {:.4f}".format(
                i, temp_count, temp_psnr / temp_count, temp_ssim / temp_count, temp_rmse / temp_count, temp_time / temp_count
            ))
			psnr += temp_psnr
			ssim += temp_ssim
			rmse += temp_rmse
			output_count += temp_count

		mean_psnr = psnr / output_count
		mean_ssim = ssim / output_count
		mean_rmse = rmse / output_count
		dic = {
			'type': 'test',
			'step': cur_epoch + 1,
			'psnr': mean_psnr,
			'ssim': mean_ssim
		}
		report_progress(dic)
		logger.info('[Validation {}] epoch: {:04d} psnr: {:.4f} ssim: {:.4f} rmse: {:.4f} time: {:.4f}'.format(settings.model_name, \
			cur_epoch+1, mean_psnr, mean_ssim, mean_rmse, total_time / output_count))
		val_logger.info('[Validation {}] epoch: {:04d} psnr: {:.4f} ssim: {:.4f} rmse: {:.4f} time: {:.4f}'.format(
            settings.model_name, cur_epoch + 1, mean_psnr, mean_ssim, mean_rmse, total_time / output_count))
	return mean_psnr, mean_ssim, mean_rmse


def seed_worker(worker_id):
	worker_seed = settings.manual_random_seed
	np.random.seed(worker_seed)
	random.seed(worker_seed)

def main():
	network = RecurrentAE(8)
	if torch.cuda.is_available():
		network.cuda()

	optimizer = torch.optim.Adam(network.parameters(), lr=0.0005, betas=(0.9, 0.99), eps=1e-08, weight_decay=0)
	scheduler = torch.optim.lr_scheduler.MultiStepLR(optimizer, milestones=[200, 800], gamma=0.5)

	checkpoint_path = os.path.join('/apdcephfs/share_1330077/ulyseszhang/save3/rae_BistroExterior_p1/RAE/RAE_latest.pth')
	load_checkpoints(network,checkpoint_path)

	if settings.resume:
		resume_model_path = os.path.join(settings.save_model_dir, settings.model_name,
                                       settings.model_name + '_latest.pth')
		checkpoint = torch.load(resume_model_path)
		start_epoch = checkpoint['epoch'] + 1
		network.load_state_dict(checkpoint['state_dict'])
		optimizer.load_state_dict(checkpoint['optimizer'])
		for state in optimizer.state.values():
			for k, v in state.items():
				if torch.is_tensor(v):
					state[k] = v.cuda()
		print('Resume training! start from epoch: ', start_epoch)
		logger.info('Resume training! start from epoch: ' + str(start_epoch))
	else:
		start_epoch = 0

	checkpoint_root_dir = os.path.join(settings.save_model_dir, settings.model_name)
	if os.path.exists(checkpoint_root_dir):
		os.rename(checkpoint_root_dir, checkpoint_root_dir + '_archived_' + time.strftime("%Y-%m-%d_%H:%M:%S", time.localtime()))

	os.makedirs(checkpoint_root_dir, exist_ok=True)
	#copy_tree('/jizhi/jizhi2/worker/trainer/rae', os.path.join(checkpoint_root_dir, 'code'))

	val_root_dir = os.path.join(checkpoint_root_dir, "val_results_latest")
	if not os.path.exists(val_root_dir):
		os.makedirs(val_root_dir)

	fh = logging.FileHandler(os.path.join(checkpoint_root_dir, 'log.txt'))
	fh.setLevel(logging.DEBUG)
	logger.addHandler(fh)
	logger.info(">>>>> RAE Denoising {} <<<<<".format(settings.model_name))
	# logger.info(network)

	train_params = {'batch_size': settings.batch_size,
					'shuffle': True,
					'pin_memory': True,
					'num_workers': settings.num_workers,
					'drop_last': True,
					'worker_init_fn': seed_worker,
					'prefetch_factor': 5,
					'generator': g}

	val_params = {'batch_size': 1,
				   'shuffle': False,
				   'pin_memory': True,
				   'num_workers': 0,
				   'worker_init_fn': seed_worker,
					'generator': g}

	training_set = ExrDataset(settings.train_root_dir, is_train=True)
	training_generator = DataLoader(training_set, **train_params)

	validation_set = ExrDataset(settings.val_root_dir, is_train=False)
	val_generator = DataLoader(validation_set, **val_params)

	best_psnr = -1
	best_ssim = -1
	best_rmse = float('inf')
	for epoch in range(start_epoch, settings.epoch_num):
		network.train()
		train(epoch, nn.DataParallel(network) if settings.multi_gpu else network, optimizer, training_generator)
			
		network.eval()
		val_psnr, val_ssim, val_rmse = val(epoch, network, val_generator, save_root_dir=val_root_dir)
		
		if val_psnr >= best_psnr:
			logger.info(
                ">>>>> At epoch {:04d}, Save Best PSNR Model: PSNR {:.4f}, SSIM {:.4f}, RMSE {:.4f}. <<<<<".format(
                    epoch + 1, val_psnr, val_ssim, val_rmse))
			copy_tree(os.path.join(settings.save_image_dir, settings.model_name, "val_results_latest"),
                      os.path.join(settings.save_image_dir, settings.model_name, "val_results_best_psnr"))
			save_model(epoch, network, optimizer, checkpoint_root_dir,
                       save_model_path=os.path.join(checkpoint_root_dir, settings.model_name + '_best-psnr.pth'))
			best_psnr = val_psnr
			
		if val_ssim >= best_ssim:
			logger.info(
                ">>>>> At epoch {:04d}, Save Best SSIM Model: PSNR {:.4f}, SSIM {:.4f}, RMSE {:.4f}. <<<<<".format(
                    epoch + 1, val_psnr, val_ssim, val_rmse))
			copy_tree(os.path.join(settings.save_image_dir, settings.model_name, "val_results_latest"),
                      os.path.join(settings.save_image_dir, settings.model_name, "val_results_best_ssim"))
			save_model(epoch, network, optimizer, checkpoint_root_dir,
                       save_model_path=os.path.join(checkpoint_root_dir, settings.model_name + '_best-ssim.pth'))
			best_ssim = val_ssim
			
		if val_rmse < best_rmse:
			logger.info(
                ">>>>> At epoch {:04d}, Save Best RMSE Model: PSNR {:.4f}, SSIM {:.4f}, RMSE {:.4f}. <<<<<".format(
                    epoch + 1, val_psnr, val_ssim, val_rmse))
			copy_tree(os.path.join(settings.save_image_dir, settings.model_name, "val_results_latest"),
                      os.path.join(settings.save_image_dir, settings.model_name, "val_results_best_rmse"))
			save_model(epoch, network, optimizer, checkpoint_root_dir,
                       save_model_path=os.path.join(checkpoint_root_dir, settings.model_name + '_best-rmse.pth'))
			best_rmse = val_rmse
			
		latest_save_model_path = os.path.join(checkpoint_root_dir, settings.model_name + '_latest.pth')
		save_model(epoch, network, optimizer, checkpoint_root_dir, save_model_path=latest_save_model_path)
		scheduler.step()
	
		
	logger.info("Synthesis val best results images to video...")
	os.system("ffmpeg -apply_trc iec61966_2_1 -i {} {}".format(
        os.path.join(settings.save_image_dir, settings.model_name, "val_results_best_psnr", "%04d.exr"),
        os.path.join(settings.save_image_dir, settings.model_name, "val_results_best_psnr", "val_best_psnr.mp4")
    ))
	os.system("ffmpeg -apply_trc iec61966_2_1 -i {} {}".format(
        os.path.join(settings.save_image_dir, settings.model_name, "val_results_best_ssim", "%04d.exr"),
        os.path.join(settings.save_image_dir, settings.model_name, "val_results_best_ssim", "val_best_ssim.mp4")
    ))
	os.system("ffmpeg -apply_trc iec61966_2_1 -i {} {}".format(
        os.path.join(settings.save_image_dir, settings.model_name, "val_results_best_rmse", "%04d.exr"),
        os.path.join(settings.save_image_dir, settings.model_name, "val_results_best_rmse", "val_best_rmse.mp4")
    ))


if __name__ == '__main__':
	main()
