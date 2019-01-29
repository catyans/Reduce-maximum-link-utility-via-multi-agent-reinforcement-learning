from socket import*
import datetime
import os
import sys
import math
import tensorflow as tf
import numpy as np
import utilize
from Network.actor import ActorNetwork
from Network.critic import CriticNetwork
from ReplayBuffer.replaybuffer import PrioritizedReplayBuffer, ReplayBuffer
from Summary.summary import Summary
from Explorer.explorer import Explorer
from SimEnv.Env1110 import Env
from flag import FLAGS
import json
import timeit

if not hasattr(sys, 'argv'):
    sys.argv  = ['']

TIME_STAMP = str(datetime.datetime.now())
SERVER_PORT = getattr(FLAGS, 'server_port')
SERVER_IP = getattr(FLAGS, 'server_ip')

OFFLINE_FLAG = getattr(FLAGS, 'offline_flag')
ACT_FLAG = getattr(FLAGS, 'act_flag')
SEED = getattr(FLAGS, 'random_seed')

ACTOR_LEARNING_RATE = getattr(FLAGS, 'learning_rate_actor')
CRITIC_LEARNING_RATE = getattr(FLAGS, 'learning_rate_critic')

GAMMA = getattr(FLAGS, 'gamma')
TAU = getattr(FLAGS, 'tau')
ALPHA = getattr(FLAGS, 'alpha')
BETA = getattr(FLAGS, 'beta')
MU = getattr(FLAGS, 'mu')
DELTA = getattr(FLAGS, 'delta')

EP_BEGIN = getattr(FLAGS, 'epsilon_begin')
EP_END = getattr(FLAGS, 'epsilon_end')
EP_ST = getattr(FLAGS, 'epsilon_steps')

ACTION_BOUND = getattr(FLAGS, 'action_bound')

BUFFER_SIZE = getattr(FLAGS, 'size_buffer')
MINI_BATCH = getattr(FLAGS, 'mini_batch')

MAX_EPISODES = getattr(FLAGS, 'episodes')
MAX_EP_STEPS = getattr(FLAGS, 'epochs')

if getattr(FLAGS, 'stamp_type') == '__TIME_STAMP':
    REAL_STAMP = TIME_STAMP
else:
    REAL_STAMP = getattr(FLAGS, 'stamp_type')
DIR_SUM = getattr(FLAGS, 'dir_sum').format(REAL_STAMP)
DIR_RAW = getattr(FLAGS, 'dir_raw').format(REAL_STAMP)
DIR_MOD = getattr(FLAGS, 'dir_mod').format(REAL_STAMP)
DIR_LOG = getattr(FLAGS, 'dir_log').format(REAL_STAMP)
DIR_CKPOINT = getattr(FLAGS, 'dir_ckpoint').format(REAL_STAMP)

AGENT_TYPE = getattr(FLAGS, "agent_type")

DETA_W = getattr(FLAGS, "deta_w")
DETA_L = getattr(FLAGS, "deta_l") # for multiagent deta_w < deta_l

EXP_EPOCH = getattr(FLAGS, "explore_epochs")
EXP_DEC = getattr(FLAGS, "explore_decay")

CKPT_PATH = getattr(FLAGS, "ckpt_path")
INFILE_NAME = getattr(FLAGS, "file_name")
INFILE_PATHPRE = "../inputs/"

IS_TRAIN = getattr(FLAGS, "is_train")

START_INDEX = getattr(FLAGS, "train_start_index")

STATE_NORMAL = getattr(FLAGS, "state_normal")

def clock(func):
    def clocked(*args):
        t0 = timeit.default_timer()
        result = func(*args)
        elapsed = timeit.default_timer() - t0
        return result, elapsed * 1e3
    return clocked
                                                          

class DrlAgent:
    def __init__(self, state_init, action_init, dim_state, dim_action, num_paths, exp_action, sess):
        self.__dim_state = dim_state
        self.__dim_action = dim_action

        self.__actor = ActorNetwork(sess, dim_state, dim_action, ACTION_BOUND,
                                    ACTOR_LEARNING_RATE, TAU, num_paths)
        self.__critic = CriticNetwork(sess, dim_state, dim_action,
                                      CRITIC_LEARNING_RATE, TAU, self.__actor.num_trainable_vars)
                                      
        self.__prioritized_replay = PrioritizedReplayBuffer(BUFFER_SIZE, MINI_BATCH, ALPHA, MU, SEED)
        self.__replay = ReplayBuffer(BUFFER_SIZE, SEED) # being depretated ?
        
        
        self.__explorer = Explorer(EP_BEGIN, EP_END, EP_ST, dim_action, num_paths, SEED, exp_action, EXP_EPOCH, EXP_DEC)


        self.__state_curt = state_init
        self.__action_curt = action_init
        self.__base_sol = utilize.get_base_solution(dim_action) # depretated

        self.__episode = 0
        self.__step = 0
        self.__ep_reward = 0.
        self.__ep_avg_max_q = 0.

        self.__beta = BETA

        self.__detaw = DETA_W
        self.__detal = DETA_L

        self.__maxutil = 100.
    
    def target_paras_init(self):
        # can be modified
        self.__actor.update_target_paras()
        self.__critic.update_target_paras()
        
    @property
    def timer(self):
        return '| %s '%ACT_FLAG \
               + '| tm: %s '%datetime.datetime.now() \
               + '| ep: %.4d '%self.__episode \
               + '| st: %.4d '%self.__step
    
    @clock
    def predict(self, state_new, reward, maxutil):
        if self.__maxutil > maxutil:
            self.__maxutil = maxutil
            self.__explorer.setExpaction(self.__action_curt)

        self.__step += 1
        self.__ep_reward += reward

        if self.__step >= MAX_EP_STEPS:
            self.__step = 0
            self.__episode += 1
            self.__ep_reward = 0.
            self.__ep_avg_max_q = 0.
            if OFFLINE_FLAG:
                self.__explorer.setEp(EP_BEGIN * 0.625)
                self.__explorer.setExpaction(self.__action_curt)
                self.__maxutil = 100.

        action_original = self.__actor.predict([state_new])[0]

        action = self.__explorer.get_act(action_original, self.__episode, flag=ACT_FLAG)

        # Priority
        if IS_TRAIN:
            target_q = self.__critic.predict_target(
                [state_new], self.__actor.predict_target([state_new]))[0]
            value_q = self.__critic.predict([self.__state_curt], [self.__action_curt])[0]
            grads = self.__critic.calculate_gradients([self.__state_curt], [self.__action_curt])
            td_error = abs(reward + GAMMA * target_q - value_q)

            transition = (self.__state_curt, self.__action_curt, reward, state_new)
            self.__prioritized_replay.add(transition, td_error, [np.mean(np.abs(grads[0]))]) 
            self.__replay.add(transition[0], transition[1], transition[2], transition[3])

        self.__state_curt = state_new
        self.__action_curt = action

        if len(self.__prioritized_replay) > MINI_BATCH and IS_TRAIN:
            curr_q = self.__critic.predict_target([state_new], self.__actor.predict([state_new]))[0]
            if curr_q[0] > target_q[0]:
                self.train(True)
            else:
                self.train(False)

        return action

    def train(self, curr_stat):
        self.__beta += (1-self.__beta) / EP_ST
        
        batch, weights, indices = self.__prioritized_replay.select(self.__beta)
        weights = np.expand_dims(weights, axis=1)

        batch_state = []
        batch_action = []
        batch_reward = []
        batch_state_next = []
        for val in batch:
            try:
                batch_state.append(val[0])
                batch_action.append(val[1])
                batch_reward.append(val[2])
                batch_state_next.append(val[3])
            except TypeError:
                print('*'*20)
                print('--val--', val)
                print('*'*20)
                continue

        target_q = self.__critic.predict_target(
            batch_state_next, self.__actor.predict_target(batch_state_next))
        value_q = self.__critic.predict(batch_state, batch_action)

        batch_y = []
        batch_error = []
        for k in range(len(batch_reward)):
            target_y = batch_reward[k] + GAMMA * target_q[k]
            batch_error.append(abs(target_y - value_q[k]))
            batch_y.append(target_y)

        predicted_q, _ = self.__critic.train(batch_state, batch_action, batch_y, weights)

        self.__ep_avg_max_q += np.amax(predicted_q)

        a_outs = self.__actor.predict(batch_state)
        grads = self.__critic.calculate_gradients(batch_state, a_outs)

        # Prioritized
        self.__prioritized_replay.priority_update(indices,
                                                  np.array(batch_error).flatten(),
                                                  np.mean(np.abs(grads[0]), axis=1))
        weighted_grads = weights * grads[0]
        # curr_stat = True means the agent is more probably in a right direction, otherwise in a wrong direction (need a larger gradient to chase other agent)
        if curr_stat:
            weighted_grads *= self.__detaw
        else:
            weighted_grads *= self.__detal
        self.__actor.train(batch_state, weighted_grads)

        self.__actor.update_target_paras()
        self.__critic.update_target_paras()
        
'''initial part'''
print("\n----Information list----")
print("agent_type: %s" % (AGENT_TYPE))
print("stamp_type: %s" % (REAL_STAMP))
UPDATE_TIMES = 0

if not OFFLINE_FLAG:
    ryuServer = (SERVER_IP, SERVER_PORT)
    tcpSocket = socket(AF_INET, SOCK_STREAM)
    tcpSocket.connect(ryuServer)

assert INFILE_NAME != "", "INFILE NAME error"  
INFILE_PATH = INFILE_PATHPRE + INFILE_NAME + ".txt"

env = Env(INFILE_PATHPRE, INFILE_NAME, INFILE_NAME.split("_")[0], MAX_EP_STEPS, SEED, START_INDEX)
#get initial info
env.showInfo() 
# Remove single-path session
NODE_NUM, SESS_NUM_ORG, EDGE_NUM, NUM_PATHS_ORG, SESS_PATHS_ORG, EDGE_MAP = env.getInfo() # SESS_PATHS shows the nodes in each path of each session
SESS_NUM = 0
NUM_PATHS = []
SESS_PATHS = []
for i in range(SESS_NUM_ORG):
    if NUM_PATHS_ORG[i] == 1:
        continue
    SESS_NUM += 1
    NUM_PATHS.append(NUM_PATHS_ORG[i])
    SESS_PATHS.append(SESS_PATHS_ORG[i])
sess_src = [SESS_PATHS[i][0][0] for i in range(SESS_NUM)]
DIM_ACTION = sum(NUM_PATHS)
print(NUM_PATHS)

agents = []
# init routing/scheduling policy: multi_agent, drl_te, mcf, ospf, .etc
if AGENT_TYPE == "multi_agent":
    # get the original action path
    action_path = getattr(FLAGS, "action_path")
    if action_path != None:
        action_ori = []
        action_file = open(action_path, 'r')
        for i in action_file.readlines():
            action_ori.append(float(i.strip()))
        ind = 0
        action = []
        for i in range(SESS_NUM_ORG):
            if NUM_PATHS_ORG[i] > 1:
                action += action_ori[ind : ind + NUM_PATHS_ORG[i]]
            ind += NUM_PATHS_ORG[i]
    else:
        action = utilize.convert_action(np.ones(DIM_ACTION), NUM_PATHS)

    
    AGENT_NUM = max(sess_src) + 1 # here AGENT_NUM is not equal to the real valid "agent number"
    srcSessNum = [0] * AGENT_NUM
    srcPathNum = [0] * AGENT_NUM
    srcUtilNum = [0] * AGENT_NUM # sum util num for each src (sum util for each path)
    srcPaths = [[] for i in range(AGENT_NUM)]
    srcActs = [[] for i in range(AGENT_NUM)] # the initial explore actions for the agents
    actp = 0
    for i in range(len(sess_src)):
        srcSessNum[sess_src[i]] += 1
        srcPathNum[sess_src[i]] += NUM_PATHS[i]
        srcPaths[sess_src[i]].append(NUM_PATHS[i])
        srcActs[sess_src[i]] += action[actp : actp + NUM_PATHS[i]];
        actp += NUM_PATHS[i]
    
    #calculate srcUtilNum(unique)
    sess_util_tmp = [{} for i in range(AGENT_NUM)]
    for i in range(SESS_NUM):
            for j in range(NUM_PATHS[i]):
                for k in range(len(SESS_PATHS[i][j])-1):
                    enode1 = SESS_PATHS[i][j][k]
                    enode2 = SESS_PATHS[i][j][k+1]
                    id_tmp = str(enode1) + "," + str(enode2)
                    if id_tmp not in sess_util_tmp[sess_src[i]]:
                        sess_util_tmp[sess_src[i]][id_tmp] = 0
    for i in range(AGENT_NUM):
        srcUtilNum[i] = len(sess_util_tmp[i].values())
    
    # construct the distributed agents
    print("\nConstructing distributed agents ... \n")
    globalSess = tf.Session()
    for i in range(AGENT_NUM):
        print("agent %d" % i)
        if (srcSessNum[i] > 0): #valid agent
            temp_dim_s = srcUtilNum[i]
            if not STATE_NORMAL:
                temp_dim_s += 1
            state = np.zeros(temp_dim_s) 
            action = utilize.convert_action(np.ones(srcPathNum[i]), srcPaths[i])
            agent = DrlAgent(list(state), action, temp_dim_s, srcPathNum[i], srcPaths[i], srcActs[i], globalSess) 
        else:
            agent = None
        agents.append(agent)
   
    # parameters init   
    print("Running global_variables initializer ...")
    globalSess.run(tf.global_variables_initializer())
    
    # init target actor and critic para
    print("Building target network ...")
    for i in range(AGENT_NUM):
        if agents[i] != None: #valid agent
             agents[i].target_paras_init()
    
    # parameters restore
    mSaver = tf.train.Saver(tf.trainable_variables()) 
    if CKPT_PATH != None:
        print("restore paramaters...")
        mSaver.restore(globalSess, CKPT_PATH)
    
    ret_c = tuple(action) # deprecated now
    
elif AGENT_TYPE == "drl_te":
    print("\nConstructing centralized agent ... \n")
    DIM_STATE = EDGE_NUM
    state = np.zeros(DIM_STATE)
    action = utilize.convert_action(np.ones(DIM_ACTION), NUM_PATHS)
    globalSess = tf.Session()
    agent = DrlAgent(state, action, DIM_STATE, DIM_ACTION, NUM_PATHS, action, globalSess)
    agents.append(agent)
    
    # parameters init  
    print("Running global_variables initializer ...")
    globalSess.run(tf.global_variables_initializer())
    
    # build target actor and critic para
    print("Building target network ...")
    agents[0].target_paras_init()
    
    # parameters restore
    mSaver = tf.train.Saver(tf.trainable_variables()) 
    if CKPT_PATH != None:
        print("restore paramaters...")
        mSaver.restore(globalSess, CKPT_PATH) 

    ret_c = tuple(action)
    
elif AGENT_TYPE == "MCF":
    action = []
    ansfile = open(getattr(FLAGS, "mcf_path"), "r")
    for i in ansfile.readlines():
        action.append(float(i.strip()))

elif AGENT_TYPE == "OBL":
    action = []
    ansfile = open(getattr(FLAGS, "obl_path"), "r")
    for i in ansfile.readlines():
        action.append(float(i.strip()))

elif AGENT_TYPE == "OR":
    action = []
    ansfile = open(getattr(FLAGS, "or_path"), "r")
    for i in ansfile.readlines():
        action.append(float(i.strip()))

else: # for OSPF
    action = utilize.convert_action(np.ones(DIM_ACTION), NUM_PATHS) # in fact only one path for each session

if not os.path.exists(DIR_LOG):
    os.mkdir(DIR_LOG)
if not os.path.exists(DIR_CKPOINT):
    os.mkdir(DIR_CKPOINT)


file_sta_out = open(DIR_LOG + '/sta.log', 'w', 1)
file_rwd_out = open(DIR_LOG + '/rwd.log', 'w', 1)
file_act_out = open(DIR_LOG + '/act.log', 'w', 1)
# file record the max util
file_util_out = open(DIR_LOG + '/util.log', 'w', 1) 
# record the rwd for each agent
file_multirwd_out = open(DIR_LOG + '/multirwd.log', 'w', 1) 
file_time_out = open(DIR_LOG + '/time.log', 'w', 1)



def split_arg(max_util, sess_path_util, net_util):
    print(max_util, file=file_util_out)
    if AGENT_TYPE == "multi_agent":
        sess_util_tmp = [{} for i in range(AGENT_NUM)]
        # link utilizations for each agent (the repeat utilizations in the same agent has been uniqued)
        sess_util = [[] for i in range(AGENT_NUM)] 
        # max link utilization for each session of each agent
        maxsess_util = [[] for i in range(AGENT_NUM)] 
        # get sess_util
        for i in range(SESS_NUM):
            for j in range(NUM_PATHS[i]):
                for k in range(len(SESS_PATHS[i][j])-1):
                    enode1 = SESS_PATHS[i][j][k]
                    enode2 = SESS_PATHS[i][j][k+1]
                    id_tmp = str(enode1) + "," + str(enode2)
                    if id_tmp not in sess_util_tmp[sess_src[i]]:
                        sess_util_tmp[sess_src[i]][id_tmp] = sess_path_util[i][j][k]
                        sess_util[sess_src[i]].append(sess_path_util[i][j][k])
        
        # get maxsess_util
        for i in range(SESS_NUM):
            temp_sessmax = 0.
            for j in sess_path_util[i]:
                temp_sessmax = max(temp_sessmax, max(j))
            maxsess_util[sess_src[i]].append(temp_sessmax)
        
        # calculate state_new, and reward for each agent
        multi_state_new = []
        multi_reward = []
        for i in range(AGENT_NUM):
            if(agents[i] == None):
                state_new = None
                reward = None
            else:
                if not STATE_NORMAL:
                    state_new = sess_util[i] + [max_util]
                else:
                    state_new = list(np.array(sess_util[i]) / max_util)
                reward = - 0.05 * (np.mean(maxsess_util[i]) + DELTA / len(maxsess_util[i]) * max_util)
            multi_state_new.append(state_new)
            multi_reward.append(reward)
        return multi_state_new, multi_reward 
    else:
        reward = - 0.05 * max_util
        state_new = list(np.array(net_util) / max_util) 
        return [state_new], [reward]

def step(max_util, sess_path_util, net_util):
    print("max_util: ", max_util)
    state, reward = split_arg(max_util, sess_path_util, net_util)
    times = []
    if AGENT_TYPE == "multi_agent":
        ret_c_t = []
        ret_c = []
        for i in range(AGENT_NUM):
            if agents[i] != None:
                result, elapsed = agents[i].predict(state[i], reward[i], max_util)  
                ret_c_t.append(result)
                times.append(elapsed)
            else:
                ret_c_t.append([])
                times.append(None)
        for i in range(len(sess_src)):
            ret_c += ret_c_t[sess_src[i]][0:NUM_PATHS[i]]
            ret_c_t[sess_src[i]] = ret_c_t[sess_src[i]][NUM_PATHS[i]:]
    elif AGENT_TYPE == "drl_te":
        result, elapsed = agents[0].predict(state[0], reward[0], max_util) 
        ret_c = tuple(result)
        times.append(elapsed)
    else:
        times.append(0.)
        # for OSPF and MCF and OBL and OR
        ret_c = tuple(action)


    reward_t = 0.
    for i in reward:
        if i != None:
            # for multiagent each agent has a reward
            reward_t += i 

    if not IS_TRAIN:
        #print(state, file=file_sta_out)
        print(ret_c, file=file_act_out)
        times_str = list(map(str, times))
        print(' '.join(times_str), file=file_time_out)
    print(reward_t, file=file_rwd_out) 
    print(reward, file=file_multirwd_out)
    return ret_c

def parse_msg(msg):
    env_msg = msg.split(';')[1]  
    env_states = json.loads(env_msg)

    max_util = env_states['max_util']
    sess_path_util = env_states['sess_path_util']
    net_util = None
    
    return max_util, sess_path_util, net_util


if __name__ == "__main__":
    print("\n----Start agent----")
    UPDATE_TIMES = 0
    route = []
    if OFFLINE_FLAG:
        for i in range(MAX_EPISODES * MAX_EP_STEPS):
            print("\n< step %d >" % (UPDATE_TIMES))
            UPDATE_TIMES += 1
            max_util, sess_path_util_org, net_util = env.update(route)
            
            if AGENT_TYPE == "OR":
                print("maxutil:", max_util)
                route = action
                continue
            
            sess_path_util = []
            for j in range(SESS_NUM_ORG):
                if NUM_PATHS_ORG[j] > 1:
                    sess_path_util.append(sess_path_util_org[j])
            ret_c = step(max_util, sess_path_util, net_util) 
            ret_c_count = 0
            route = []
            for j in range(SESS_NUM_ORG):
                if NUM_PATHS_ORG[j] > 1:
                    for k in range(NUM_PATHS_ORG[j]):
                        route.append(round(ret_c[ret_c_count], 3))
                        ret_c_count += 1
                else:
                    route.append(1.0)
            
    else: 
        blockSize = 1024
        BUFSIZE = 1025
        while True:
            msgTotalLen = 0
            msgRecvLen = 0
            msg = ""
            finish_flag = False
            # recv environment state
            while True:
                datarecv = tcpSocket.recv(BUFSIZE).decode()
                if len(datarecv) > 0:
                    if msgTotalLen == 0:
                        totalLenStr = (datarecv.split(';'))[0]
                        msgTotalLen = int(totalLenStr) + len(totalLenStr) + 1#1 is the length of ';'
                        if msgTotalLen == 2:#stop signal
                            print("simulation is over!")
                            finish_flag = True
                            break
                    msgRecvLen += len(datarecv)
                    msg += datarecv
                    if msgRecvLen == msgTotalLen: 
                        break
            
            if finish_flag:
                break;
            print("\n< step %d >" % (UPDATE_TIMES))
            UPDATE_TIMES += 1
            
            # calculate the action according to the environment 
            max_util, sess_path_util_org, net_util = parse_msg(msg)
            
            sess_path_util = []
            for j in range(SESS_NUM_ORG):
                if NUM_PATHS_ORG[j] > 1:
                    sess_path_util.append(sess_path_util_org[j])
            ret_c = step(max_util, sess_path_util, net_util) # not round
            
            ret_c_count = 0
            route = []
            for j in range(SESS_NUM_ORG):
                if NUM_PATHS_ORG[j] > 1:
                    for k in range(NUM_PATHS_ORG[j]):
                        route.append(round(ret_c[ret_c_count], 3))
                        ret_c_count += 1
                else:
                    route.append(1.0)
            
            msg = ""
            msg += json.dumps(route)
            msg = str(len(msg)) + ';' + msg;

            msgTotalLen = len(msg)
            blockNum = int((msgTotalLen+blockSize-1)/blockSize);
            for i in range(blockNum):
                data = msg[i*blockSize:i*blockSize+blockSize]
                tcpSocket.send(data.encode())     
    
    # store global variables
    if (AGENT_TYPE == "multi_agent" or AGENT_TYPE == "drl_te") and IS_TRAIN:  
        print("saving checkpoint...")
        mSaver = tf.train.Saver(tf.global_variables())        
        mSaver.save(globalSess, DIR_CKPOINT + "/ckpt")
        print(DIR_CKPOINT)
