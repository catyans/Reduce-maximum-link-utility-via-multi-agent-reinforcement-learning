# topology: 
# EXP:
#       s2----s4
#       |     |
# s1----|     |----s6
#       |     |
#       s3----s5
#CER:
# ------s1----s2
# |     |     |
# |     |     |
# |     |     |
# s5----s4----s3
#    |
#    |
#    s6

from ryu.base import app_manager
from ryu.controller import ofp_event
from ryu.controller.handler import CONFIG_DISPATCHER, MAIN_DISPATCHER, DEAD_DISPATCHER
from ryu.controller.handler import set_ev_cls
from ryu.ofproto import ofproto_v1_3
from ryu.lib.packet import packet
from ryu.lib.packet import ether_types
from ryu.lib.packet import ethernet, ipv4, arp
from ryu.lib import hub
from ryu import cfg
from operator import attrgetter
import socket
import json
from sorsolver import *
import os
import sys

class FlowScheduler(app_manager.RyuApp):
    OFP_VERSIONS = [ofproto_v1_3.OFP_VERSION]

    def __init__(self, *args, **kwargs):
        super(FlowScheduler, self).__init__(*args, **kwargs)
        self.swith_host_port = None
        self.link_port = None
        self.node_num = None
        self.host_num = None
        self.capa_matrix = []
        self.sess_paths = []
        self.max_path_num = None
        self.sess_rate = []
        self.sess_rates = []
        self.monitor_bytes = [] # a node_num * node_num matrix record the current point to point bytes
        self.monitor_bytes_new = []
        self.monitor_path_rates = []
        self.path_num = []
        self.sess_num = None
        # switch para
        self.dpid_dppath = {}
        self.rule_ready_flag =False
        self.rule_priority = None
        self.firewall_priority = None
        self.bytes_map = []
        self.rates_map = []
        self.total_bytes = []
        self.throughput = []
        self.max_util = 0.0
        self.port_states_reply_count = 0
        self.flow_states_reply_count = 0
        # connection setting to the real switch
        self.server_socket = None
        self.host_sockets = []
        self.server_IP = ""
        self.server_port = None
        self.conn_ready_flag = False
        # experiment setting
        self.method = None 
        self.monitor_period = None
        self.update_count = 0
        self.rate_update_count = 0
        self.actions = []
        self.stop_flag = False
        self.max_update_count = 0
        self.update_interval = 0
        self.rate_start_index = 0 # the starting sess rate index for testing

        self.set_para() # must before initiate()
        self.initiate()
        self.listen_thread = hub.spawn(self.listen_hosts)
        self.install_rules_thread = hub.spawn(self.install_static_rules)
        self.monitor_thread = hub.spawn(self.monitor_stats)
        self.update_rates_thread = hub.spawn(self.update_rates)
        
        # connection setting for sim-ddpg or other local agent
        self.local_server_socket = None
        self.local_server_IP = "127.0.0.1"
        self.local_server_port = 50001
        self.ddpg_agent = None
        self.agent_ready_flag = False
        if self.method == "OBL":
            self.agent_listen_thread = hub.spawn(self.listen_agent)

        self.blockSize = 1024
        self.BUFSIZE = 1025

    def __del__(self):
        self.server_socket.close()
        self.local_server_socket.close()

    '''set_para(): set some basic parameters'''
    def set_para(self):
        CONF = cfg.CONF
        CONF.register_opts([
            cfg.StrOpt('method', default="SOR", help=("agent method")),
            cfg.StrOpt('logging_stamp', default="test", help=("logging directory name"))])

        self.method = CONF.method # OBL, SHR, OR, LB, SOR
        self.server_IP = "192.168.0.134"
        self.server_port = 8888
        self.rule_priority = 4
        self.firewall_priority = 10
        self.max_update_count = 288 * 8 + 40
        self.max_path_num = 4
        
        # setting experiment setting
        self.logging_stamp = CONF.logging_stamp
        self.update_interval = 12
        # monitoring interval time (second)
        self.monitor_period = 3
        self.rate_start_index  = 864

    '''initiate(): some initials executed when this application starts'''
    def initiate(self):
        self.logging_init()
        self.set_topo_info()
        self.get_paths()
        self.init_actions()
    
    '''set_topo_info(): init some static topology information'''
    def set_topo_info(self):
        self.node_num = 6
        self.host_num = 6
        self.bytes_map = [[0]*self.node_num for i in range(self.node_num)]
        self.rates_map = [[0.0]*self.node_num for i in range(self.node_num)]
        self.total_bytes = [0]*self.node_num
        self.throughput = [0.0]*self.node_num
        self.host_sockets = [None for i in range(self.host_num)]

        self.swith_host_port = {0x0122:1, 0x0121:13, 0x0125:23, 
                                0x0123:2, 0x0124:14, 0x0126:24}
        self.link_port = [[-1,7,-1,9,15,-1],
                          [5,-1,3,-1,-1,-1],
                          [-1,4,-1,6,-1,-1],
                          [10,-1,8,-1,12,16],
                          [17,-1,-1,19,-1,21],
                          [-1,-1,-1,18,22,-1]]
        for i in range(self.node_num):
            self.capa_matrix.append([])
            for j in range(self.node_num):
                if i == j:
                    self.capa_matrix[i].append(0.0)
                else:
                    self.capa_matrix[i].append(100.0)

    '''get_paths(): read session paths from txt files'''
    def get_paths(self):
        filename = None
        if self.method == "OBL" or self.method == "LB" or self.method == "SOR":
            filename = "EXP_OBL_3_0_4031"
        elif self.method == "SHR":
            filename = "EXP_SHR_1_0_4031"
        elif self.method == "OR":
            filename = "EXP_ORI_100_0_4031"
        else:
            print("ERROR: no such scheduling method!")
            exit()
        filein = open("pathfiles/" + filename + ".txt")
        lines = filein.readlines()
        filein.close()
        flag = False
        sesspath = []
        for i in range(1, len(lines)):
            if lines[i].strip() == "succeed":
                flag = True
                self.path_num.append(len(sesspath))
                self.sess_paths.append(sesspath)
                continue
            if not flag:
                lineList = lines[i].strip().split(',')
                if len(sesspath) != 0 and (int(lineList[1]) != sesspath[0][0] or int(lineList[-2]) != sesspath[0][-1]):
                    self.path_num.append(len(sesspath))
                    self.sess_paths.append(sesspath)
                    sesspath = []
                sesspath.append(list(map(int, lineList[1:-1])))
            else:
                sessrate = list(map(float, lines[i].strip().split(',')))
                self.sess_rates.append(sessrate)

        self.sess_num = len(self.sess_paths)
        if max(self.path_num) > self.max_path_num:
            print("ERROR:multipath number should not exceed 4")
            exit()

        self.sess_rate = [10]*self.sess_num
        for i in range(self.node_num):
            self.monitor_bytes.append([0] * self.node_num)
            self.monitor_bytes_new.append([0] * self.node_num)
        self.monitor_path_rates = [0] * sum(self.path_num)


    '''init_actions(): init actions for every flow scheduling method'''
    def init_actions(self):
        if self.method == "OBL" or self.method == "LB" or self.method == "SOR":
            self.actions = []
            for item in self.path_num:
                for i in range(item):
                    self.actions.append(round(1.0/item, 4))
        elif self.method == "SHR":
            self.actions = [1.0 for i in range(self.sess_num)]
        elif self.method == "OR":
            filein = open("pathfiles/EXP_ORI_100_solu0.txt")
            lines = filein.readlines()
            filein.close()
            self.actions = [round(float(lines[i].strip()), 4) for i in range(len(lines))]
        else:
            print("ERROR: no such scheduling method!")
            exit()

    '''create_sockets(): wait connections from each host'''
    def listen_hosts(self):
        self.server_socket = socket.socket()
        self.server_socket.bind((self.server_IP,self.server_port))
        self.server_socket.listen(self.host_num)
        conn_num = 0
        print("wait for connection...")
        while True:
            conn,addr = self.server_socket.accept()
            hostid = int(addr[0].split('.')[-1]) - 121
            self.host_sockets[hostid] = conn
            conn_num += 1
            print("Host %d connected" % hostid)

            if conn_num >= self.host_num:
                self.conn_ready_flag = True
                break
        msgs = ["0;" for i in range(self.host_num)]
        for i in range(self.sess_num):
            src = self.sess_paths[i][0][0]
            dst = self.sess_paths[i][0][-1]
            msgs[src] += str(dst) + ',' + str(self.path_num[i]) + "," + str(self.sess_rate[i]) + ","

        for i in range(self.host_num):
            self.host_sockets[i].send((msgs[i] + str(self.max_update_count) + ",#").encode())

    '''listen_agent(): wait ddpg agent connected'''
    def listen_agent(self):
        self.local_server_socket = socket.socket()
        self.local_server_socket.bind((self.local_server_IP, self.local_server_port))
        self.local_server_socket.listen(1)

        conn, addr = self.local_server_socket.accept()
        self.ddpg_agent = conn
        self.agent_ready_flag = True
        print("Connected with sim-ddpg agent successfully!")


    '''update_rates(): update session rates to each host'''
    def update_rates(self):
        while self.update_count < 10:
            hub.sleep(1)
        while self.update_count < self.max_update_count-1:
            rates = self.sess_rates[(self.rate_start_index + self.rate_update_count) % len(self.sess_rates)]
            self.rate_update_count += 1
            self.sess_rate = []
            for i in range(self.sess_num):
                rate = round(rates[i] / 100)
                self.sess_rate.append(rate)
            msgs = ["2;" for i in range(self.host_num)]
            for i in range(self.sess_num):
                src = self.sess_paths[i][0][0]
                dst = self.sess_paths[i][0][-1]
                msgs[src] += str(dst) + ',' + str(self.sess_rate[i]) + ","

            for i in range(self.host_num):
                self.host_sockets[i].send((msgs[i] + "#").encode())
            ##break
            hub.sleep(self.update_interval)

    '''monitor_stats(): send stats request to each switch every a few seconds'''
    def monitor_stats(self):
        while True:
            if self.conn_ready_flag and self.rule_ready_flag:
                break
            hub.sleep(self.monitor_period)
        print("Enter into monitor state")
        while not self.stop_flag:
            for datapath in self.dpid_dppath.values():
                # self.logger.debug('send stats request: %016x', datapath.id)
                ofproto = datapath.ofproto
                parser = datapath.ofproto_parser
                req = parser.OFPFlowStatsRequest(datapath)
                datapath.send_msg(req)
                req = parser.OFPPortStatsRequest(datapath, 0, ofproto.OFPP_ANY)
                datapath.send_msg(req)
            # detection period
            print("monitor_states request send!")
            hub.sleep(self.monitor_period)

    '''_flow_stats_reply_handler(): parse stats reply packets from each switch'''
    @set_ev_cls(ofp_event.EventOFPFlowStatsReply, MAIN_DISPATCHER)
    def _flow_stats_reply_handler(self, ev):
        body = ev.msg.body

        nodeid = ev.msg.datapath.id - 0x121
        total_byte_count = 0
        for stat in body:
            if 'ip_proto' in stat.match and stat.match['ip_proto'] == 17 and 'udp_src' in stat.match and (stat.match['udp_src'] - 8000) // 100 == nodeid:
                dst = (stat.match['udp_src'] % 100) // 10
                self.monitor_bytes_new[nodeid][dst] += stat.byte_count
            if stat.instructions == [] and 'ip_proto' in stat.match and stat.match['ip_proto'] == 17:
                total_byte_count += stat.byte_count

        self.throughput[nodeid] = round(float(total_byte_count - self.total_bytes[nodeid])*8/self.monitor_period/1000/1000, 3)
        self.total_bytes[nodeid] = total_byte_count
        
        self.flow_states_reply_count += 1
        if self.flow_states_reply_count == self.node_num:
            #calculate the monitor path rates for sorsolving
            path_rates = []
            for i in range(self.sess_num):
                src = self.sess_paths[i][0][0]
                dst = self.sess_paths[i][0][-1]
                pathnum = len(self.sess_paths[i])
                bit_rate = float(self.monitor_bytes_new[src][dst] - self.monitor_bytes[src][dst]) * 8 / self.monitor_period / 1e6
                path_rates += [bit_rate] * pathnum
            self.monitor_path_rates = path_rates
            self.monitor_bytes = self.monitor_bytes_new
            self.monitor_bytes_new = []
            for i in range(self.node_num):
                self.monitor_bytes_new.append([0] * self.node_num)
            
            self.flow_states_reply_count = 0
            print("self.total_bytes:")
            print(self.total_bytes)
            print("self.throughput:")
            print(self.throughput)
            throughput_str = " ".join(list(map(str, self.throughput)))
            print(throughput_str, file=self.file_throughput)


    '''_port_stats_reply_handler(): parse stats reply packets from each switch'''
    @set_ev_cls(ofp_event.EventOFPPortStatsReply, MAIN_DISPATCHER)
    def _port_stats_reply_handler(self, ev):
        body = ev.msg.body
        # store stats, get maps
        nodeid = ev.msg.datapath.id - 0x121
        for stat in body:
            for i in range(self.node_num):
                if stat.port_no == self.link_port[nodeid][i]:
                    self.rates_map[nodeid][i] = round((stat.tx_bytes - self.bytes_map[nodeid][i])*8.0/self.monitor_period/1000/1000, 2)
                    self.bytes_map[nodeid][i] = stat.tx_bytes
                    break

        self.port_states_reply_count += 1
        if self.port_states_reply_count == self.node_num:
            self.port_states_reply_count = 0
            states = self.compute_states()
            self.make_decision(states)

    '''compute_states(): compute states as the input of DRL'''
    def compute_states(self):
        sess_path_util = []
        maxutil = 0.0
        for i in range(len(self.path_num)): 
            sess_path_util.append([])
            for j in range(self.path_num[i]):
                pathutil = []
                for k in range(len(self.sess_paths[i][j])-1):
                    enode1 = self.sess_paths[i][j][k]
                    enode2 = self.sess_paths[i][j][k+1]
                    pathutil.append(round(self.rates_map[enode1][enode2]/self.capa_matrix[enode1][enode2], 5))
                sess_path_util[i].append(pathutil)
                maxutil = max(maxutil, max(pathutil))
        
        # calculate edge utils
        net_util = []
        for i in range(self.node_num):
            for j in range(self.node_num):
                if self.capa_matrix[i][j] > 0:
                    net_util.append(self.rates_map[i][j] / self.capa_matrix[i][j])

        self.max_util = maxutil
        print("maxutil:%f" % maxutil)

        print(self.max_util, file=self.file_maxutil)
        print(" ".join(list(map(str, net_util))), file=self.file_edgeutil)

        states = {
                'max_util': maxutil,
                'sess_path_util': sess_path_util,
                'net_util': net_util
                }
        return states

    '''make_decision(): decide current best splitting ratios of each ingress node'''
    def make_decision(self, states):
        if self.method == "OBL" and self.update_count >= 5: # wait for traffic coming
            self.actions = self.sim_ddpg(states)
            self.execute_decision(self.actions)
        elif self.method == "SOR":
            self.actions = self.get_sorsolu()
            print("current action:", self.actions)
            self.execute_decision(self.actions)
        else:
            self.execute_decision(self.actions)
    
    '''get_sorsolu: get the solution of the current traffic matrix using OBL pahts'''
    def get_sorsolu(self):
        totalPathNum = sum(self.path_num) 
        pathRates = []
        for i in range(len(self.path_num)):
            for j in range(self.path_num[i]):
                pathRates.append(self.sess_rate[i])
        return sorsolver(self.node_num, self.sess_num, totalPathNum, self.sess_paths, self.monitor_path_rates, self.capa_matrix)

    '''sim_ddpg(): input states, return actions'''
    def sim_ddpg(self, states):
        if not self.agent_ready_flag:
            print("no sim_ddpg agent")
            exit()
        state_encode = json.dumps(states)
        self.send_msg(state_encode, self.ddpg_agent) 
        recvmsg = self.recv_msg(self.ddpg_agent)
        action = json.loads(recvmsg) 
        return action

    '''send_msg(): send a str msg to the dest '''
    def send_msg(self, msg, socket):
        msg = str(len(msg)) + ';' + msg
        msgTotalLen = len(msg)
        blockNum = int((msgTotalLen - 1) / self.blockSize + 1)
        for i in range(blockNum):
            data = msg[i * self.blockSize : (i + 1) * self.blockSize]
            socket.send(data.encode())
    
    '''recv_msg(): receive the message from src'''
    def recv_msg(self, socket):
        msgTotalLen = 0
        msgRecvLen = 0
        realmsg_ptr = 0
        msg = ""
        while True:
            datarecv = socket.recv(self.BUFSIZE).decode()
            if len(datarecv) > 0:
                if msgTotalLen == 0:
                    totalLenStr = (datarecv.split(';'))[0]
                    msgTotalLen = int(totalLenStr) + len(totalLenStr) + 1#1 is the length of ';'
                    realmsg_ptr = len(totalLenStr) + 1
                msgRecvLen += len(datarecv)
                msg += datarecv
                if msgRecvLen == msgTotalLen: 
                    return msg[realmsg_ptr:]

    '''execute_decision(): send actions to each host to adjust the flow generation'''
    def execute_decision(self, actions):
        self.update_count += 1
        if self.update_count >= self.max_update_count:
            self.stop_flag = True
            for i in range(self.host_num):
                self.host_sockets[i].send(("3;stop").encode())
            
            if self.method == "OBL":
                self.ddpg_agent.send(("0;").encode())
            print("\nExperiment finished!")
            hub.sleep(30) 
            os._exit(0)
            return

        if actions == []:
            print("ERROR: actions is None")
        action_index = 0
        msgs = ["1;" for i in range(self.host_num)]
        for i in range(self.sess_num):
            src = self.sess_paths[i][0][0]
            dst = self.sess_paths[i][0][-1]
            msgs[src] += str(dst) + ','
            for j in range(self.path_num[i]):
                msgs[src] += str(actions[action_index]) + ','
                action_index += 1

        for i in range(self.host_num):
            self.host_sockets[i].send((msgs[i] + '#').encode())

        print("Update actions: %d times" % self.update_count)

    '''install_static_rules(): install static forwarding rules of each path'''
    def install_static_rules(self):
        while len(self.dpid_dppath) < self.node_num:
            hub.sleep(5)

        udp_port = 0
        in_port = 0
        # for UDP flows
        for i in range(self.sess_num):
            src = self.sess_paths[i][0][0]
            dst = self.sess_paths[i][0][-1]
            for j in range(self.path_num[i]):
                path = self.sess_paths[i][j]
                udp_port = 8000+src*100+dst*10+j
                for k in range(len(path) - 1):
                    datapath = self.dpid_dppath[0x121 + path[k]]
                    ofproto = datapath.ofproto
                    parser = datapath.ofproto_parser
                    if path[k] == src:
                        in_port = self.swith_host_port[0x121 + src]
                    else:
                        in_port = self.link_port[path[k]][path[k-1]]
                    match = parser.OFPMatch(in_port=in_port, udp_src=udp_port, ip_proto=17,
                                ipv4_dst="10.168.100.12"+str(dst+1), eth_type=0x0800)
                    out_port = self.link_port[path[k]][path[k+1]]
                    actions = [parser.OFPActionOutput(out_port)]
                    self.add_flow(datapath, self.rule_priority, match, actions)
        # for ICMP flows
        for i in range(self.sess_num):
            src = self.sess_paths[i][0][0]
            dst = self.sess_paths[i][0][-1]
            # go there
            for j in range(self.path_num[i]):
                path = self.sess_paths[i][j]
                ipv4_src = "10.168.10"+str(j)+".12"+str(src+1)
                for k in range(len(path) - 1):
                    datapath = self.dpid_dppath[0x121 + path[k]]
                    ofproto = datapath.ofproto
                    parser = datapath.ofproto_parser
                    if path[k] == src:
                        in_port = self.swith_host_port[0x121 + src]
                    else:
                        in_port = self.link_port[path[k]][path[k-1]]
                    match = parser.OFPMatch(in_port=in_port, ip_proto=1, ipv4_src=ipv4_src, 
                                ipv4_dst="10.168.100.12"+str(dst+1), eth_type=0x0800)
                    out_port = self.link_port[path[k]][path[k+1]]
                    actions = [parser.OFPActionOutput(out_port)]
                    self.add_flow(datapath, self.rule_priority, match, actions)
            # come back
            for j in range(self.path_num[i]):
                path = self.sess_paths[i][j]
                ipv4_dst = "10.168.10"+str(j)+".12"+str(src+1)
                for k in range(len(path)-1, 0, -1):
                    datapath = self.dpid_dppath[0x121 + path[k]]
                    ofproto = datapath.ofproto
                    parser = datapath.ofproto_parser
                    if path[k] == dst:
                        in_port = self.swith_host_port[0x121 + dst]
                    else:
                        in_port = self.link_port[path[k]][path[k+1]]
                    match = parser.OFPMatch(in_port=in_port, ip_proto=1, ipv4_src="10.168.100.12"+str(dst+1), 
                                ipv4_dst=ipv4_dst, eth_type=0x0800)
                    out_port = self.link_port[path[k]][path[k-1]]
                    actions = [parser.OFPActionOutput(out_port)]
                    self.add_flow(datapath, self.rule_priority, match, actions)

        self.rule_ready_flag = True
        print("Install static flow rules successfully")

    '''switch_features_handler(): install static flow rules upon connecting with each switch'''
    @set_ev_cls(ofp_event.EventOFPSwitchFeatures, CONFIG_DISPATCHER)
    def switch_features_handler(self, ev):
        datapath = ev.msg.datapath
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser

        dpid = datapath.id
        nodeid = dpid - 0x0121
        self.dpid_dppath[dpid] = datapath
        print("install static flow rules for s12"+str(nodeid+1))
        # 1.install packet-in flow rules
        match = parser.OFPMatch()
        actions = [parser.OFPActionOutput(ofproto.OFPP_CONTROLLER,
                                          ofproto.OFPCML_NO_BUFFER)]
        self.add_flow(datapath, 0, match, actions)
        # 2.install host--switch flow rules
        for i in range(self.node_num):
            if self.link_port[nodeid][i] < 0:
                continue
            in_port = self.link_port[nodeid][i]
            match = parser.OFPMatch(in_port=in_port, ipv4_dst="10.168.100.12"+str(nodeid+1), ip_proto=17, eth_type=0x0800)
            actions = []
            self.add_flow(datapath, self.rule_priority, match, actions)
            # install ICMP flow rules
            out_port = self.swith_host_port[dpid]
            actions = [parser.OFPActionOutput(out_port)]
            for j in range(self.max_path_num):
                match = parser.OFPMatch(in_port=in_port, ipv4_dst="10.168.10"+str(j)+".12"+str(nodeid+1), ip_proto=1, eth_type=0x0800)
                self.add_flow(datapath, self.rule_priority, match, actions)

        # 3.install firwall rules
        in_port = self.swith_host_port[dpid]
        actions = []
        # ipv6
        match = parser.OFPMatch(in_port=in_port, eth_type=0x86DD)
        self.add_flow(datapath, self.firewall_priority, match, actions)
        # dst = 224.*
        match = parser.OFPMatch(in_port=in_port, ipv4_dst="224.0.0.0/255.255.0.0", eth_type=0x0800)
        self.add_flow(datapath, self.firewall_priority, match, actions)
        # eth=33:*
        match = parser.OFPMatch(in_port=in_port, eth_dst="33:33:00:00:00:02")
        self.add_flow(datapath, self.firewall_priority, match, actions)
        # src = 192.168.*
        match = parser.OFPMatch(in_port=in_port, ipv4_src="192.168.0.0/255.255.0.0", eth_type=0x0800)
        self.add_flow(datapath, self.firewall_priority, match, actions)
        # dst = 192.168.*
        match = parser.OFPMatch(in_port=in_port, ipv4_dst="192.168.0.0/255.255.0.0", eth_type=0x0800)
        self.add_flow(datapath, self.firewall_priority, match, actions)


    '''add_flow(): add a flow rule into a specific switch'''
    def add_flow(self, datapath, priority, match, actions, buffer_id=None):
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser

        inst = [parser.OFPInstructionActions(ofproto.OFPIT_APPLY_ACTIONS,
                                             actions)]
        if buffer_id:
            mod = parser.OFPFlowMod(datapath=datapath, buffer_id=buffer_id,
                                    priority=priority, match=match,
                                    instructions=inst)
        else:
            mod = parser.OFPFlowMod(datapath=datapath, priority=priority,
                                    match=match, instructions=inst)
        datapath.send_msg(mod)

    '''_packet_in_handler(): deal with arp packet'''
    @set_ev_cls(ofp_event.EventOFPPacketIn, MAIN_DISPATCHER)
    def _packet_in_handler(self, ev):
        if ev.msg.msg_len < ev.msg.total_len:
            self.logger.debug("packet truncated: only %s of %s bytes",
                              ev.msg.msg_len, ev.msg.total_len)
        msg = ev.msg
        datapath = msg.datapath
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser
        
        pkt = packet.Packet(msg.data)
        arppkt = pkt.get_protocol(arp.arp)

        if arppkt != None:
            print("Packet in: arp broadcast")
            for arp_dpid in self.swith_host_port:
                actions = [parser.OFPActionOutput(self.swith_host_port[arp_dpid])]
                datapath = self.dpid_dppath[arp_dpid]
                out = parser.OFPPacketOut(
                    datapath=datapath,
                    buffer_id=ofproto.OFP_NO_BUFFER,
                    in_port=ofproto.OFPP_CONTROLLER,
                    actions=actions, data=msg.data)
                datapath.send_msg(out)
    '''logging_init(): init the logging file'''
    def logging_init(self):
        DIR_LOG = "./log/%s" % (self.logging_stamp)
        if not os.path.exists(DIR_LOG):
            os.mkdir(DIR_LOG)
        self.file_maxutil = open(DIR_LOG + "/maxutil.log", "w", 1)
        self.file_throughput = open(DIR_LOG + "/throughput.log", "w", 1)
        self.file_edgeutil = open(DIR_LOG + "/edgeutil.log", "w", 1)

