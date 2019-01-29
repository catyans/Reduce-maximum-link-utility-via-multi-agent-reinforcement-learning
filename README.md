# DATE Project

## 1 Path selection module

### 1.1 Description
"TDTE" is a project of vs2015 used for preparing input files of simulations and experiments. "topoSet" is the directory storing the generated files. 

### 1.2  How to run
The platform is windows 7 or 10. vs2015 and Gurobi 702 should be installed. 

We take the topology named "EXP" for example. In the main file of TETD project, there are some flag variables, 

OBL_3:select paths for DATE

SHR_3:select paths using KSP

MCF_3:select paths using KMCF

ORI_100:compute oblivious routing

SHR_1:compute the shortest path routing

MCF_100:compute the optimal routing

MCFb_100:compute the optimal routing with edge degradation

OBL_3:compute the optimal routing with DATE candidate path constraints

SHR_3:compute the optimal routing with KSP candidate path constraints

MCF_3:compute the optimal routing with KMCF candidate path constraints


To execute an operation, set the corresponding flag to 1. Note that, modify the variable "filePathPrefix" to the path of "topoSet" in your machine. 

### 1.3 Notes
The module is not humanized now and it will be improved in future.

## 2 Simulation

### 2.1 Environment

We use python 3.5.2 and tensorflow 1.8.0 to construct our simulation environment.

### 2.2 How to run

We give simple examples for training and testing our multi-agent DRL model off-line in

```
/DRLTE/drlte/run_offline.sh
```

to run the examples, you need to come into the code directory at first

```
cd DRLTE/drlte
```

and train one episode using

```
python3 sim-ddpg.py --epochs=19999 --episodes=1 --stamp_type=train-convergence --file_name=EXP_OBL_3_0_4031 --offline_flag=True --epsilon_steps=2700
```

or do a 3day-data training

```
python3 sim-ddpg.py --epochs=8000 --episodes=100 --stamp_type=3day-train --file_name=EXP_OBL_3_0_864_fit15_step36 --offline_flag=True --epsilon_steps=1800
```

and then use the parameters stored to test

```
python3 sim-ddpg.py --epochs=39 --episodes=4032 --stamp_type=test-all --file_name=EXP_OBL_3_0_4031 --offline_flag=True --epsilon_begin=0. --is_train=False --ckpt_path=../log/ckpoint/3day-train/ckpt
```

### 2.3 Notes

Here we use an example network topology "EXP" as input, in contrary with the topology Abi, GEA and CER in paper.

## 3 Experiment on RYU

### 3.1 Environment

We use python 3.5.2, ryu-manager to construct our control module and C language to construct the remote hosts' packet sender. Furtherer more, we use gurobi to solve the optimal problem for SMORE method. 

### 3.2 Notes

To run the experiment in this part we need complex configuration and many extra scripts.  We only provide major codes in this part in order to make it simpler and clearer.

```
the control module:
DATE.py
the solver for SMORE method:
sorsolver.py
the packet sender for the remote host:
packet_sender.c
the example input topology and path files:
pathfiles/*
```

More details for experiment part may be provided in the future.



