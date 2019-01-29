# convergence test example 
#python3 sim-ddpg.py --epochs=19999 --episodes=1 --stamp_type=train-convergence --file_name=EXP_OBL_3_0_4031 --offline_flag=True --epsilon_steps=2700  
# 3 day training example
#python3 sim-ddpg.py --epochs=8000 --episodes=100 --stamp_type=3day-train --file_name=EXP_OBL_3_0_864_fit15_step36 --offline_flag=True --epsilon_steps=1800
# test example
python3 sim-ddpg.py --epochs=39 --episodes=4032 --stamp_type=test-all --file_name=EXP_OBL_3_0_4031 --offline_flag=True --epsilon_begin=0. --is_train=False --ckpt_path=../log/ckpoint/3day-train/ckpt
