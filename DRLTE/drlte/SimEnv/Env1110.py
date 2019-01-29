import random
import numpy as np


# This Env is used for offline training.
class Env:
    def __init__(self, path_pre, file_name, topo_name, epoch, seed, start_index):
        random.seed(seed)
        np.random.seed(seed)
        self.__filepath = path_pre + file_name + ".txt"
        self.__topopath = path_pre + topo_name + ".txt"
        self.__epoch = epoch
        self.__episode = -1
        self.__nodenum = 0
        self.__edgenum = 0
        self.__edgemap = []
        self.__capaMatrix = []
        self.__sessnum = 0
        self.__sesspaths = []
        self.__pathnum = []
        self.__sessrate = []
        self.__sessrates = []
        self.__flowmap = []
        self.__updatenum = 0
        
        self.__toponame = topo_name
        self.__link_ind = 0
        self.__rate_ind = 0

        self.__start_index = start_index
        self.readFile()
        self.initFlowMap()
        
        
    
    def readFile(self):
        file = open(self.__filepath, 'r')
        lines = file.readlines()
        file.close()
        
        sesspath = []
        lineNum = 0
        for i in range(1, len(lines)):
            if lines[i].strip() == "succeed":
                lineNum = i
                break
            lineList = lines[i].strip().split(',')
            if len(sesspath) != 0 and (int(lineList[1]) != sesspath[0][0] or int(lineList[-2]) != sesspath[0][-1]):
                self.__sesspaths.append(sesspath)
                sesspath = []
            sesspath.append(list(map(int, lineList[1:-1])))
        self.__sesspaths.append(sesspath)
        self.__sessnum = len(self.__sesspaths)
        self.__pathnum = [len(item) for item in self.__sesspaths]
        for i in range(lineNum+1, len(lines)):
            sessratetmp = list(map(float, lines[i].strip().split(',')))
            self.__sessrates.append(sessratetmp)
        for item in self.__sesspaths:
            self.__nodenum = max([self.__nodenum, max([max(i) for i in item])])
        self.__nodenum += 1
        for i in range(self.__nodenum):
            self.__edgemap.append([])
            for j in range(self.__nodenum):
                self.__edgemap[i].append(0)
        for i in range(self.__sessnum):
            for j in range(self.__pathnum[i]):
                for k in range(len(self.__sesspaths[i][j])-1):
                    enode1 = self.__sesspaths[i][j][k]
                    enode2 = self.__sesspaths[i][j][k+1]
                    self.__edgemap[enode1][enode2] = 1
        for i in range(self.__nodenum):
            for j in range(self.__nodenum):
                self.__edgenum += self.__edgemap[i][j]

        topofile = open(self.__topopath, 'r')
        lines = topofile.readlines()
        topofile.close()
        for i in range(self.__nodenum):
            crow = [0]*self.__nodenum
            self.__capaMatrix.append(crow)
        for i in range(1, len(lines)):
            lineList = lines[i].strip().split(" ")
            enode1 = int(lineList[0]) - 1
            enode2 = int(lineList[1]) - 1
            self.__capaMatrix[enode1][enode2] = float(lineList[3])
            self.__capaMatrix[enode2][enode1] = float(lineList[3])
        

    def initFlowMap(self):
        for i in range(self.__nodenum):
            self.__flowmap.append([])
            for j in range(self.__nodenum):
                self.__flowmap[i].append(0)

    def getFlowMap(self, action):
        if action == []:
            for item in self.__pathnum:
                action += [round(1.0/item, 4) for j in range(item)]
        subrates = []
        count = 0
        for i in range(self.__sessnum):
            subrates.append([])
            for j in range(self.__pathnum[i]):
                tmp = 0
                if j == self.__pathnum[i] - 1:
                    tmp = self.__sessrate[i] - sum(subrates[i])
                else:
                    tmp = self.__sessrate[i]*action[count]
                count += 1
                subrates[i].append(tmp)

        for i in range(self.__nodenum):
            for j in range(self.__nodenum):
                self.__flowmap[i][j] = 0
        for i in range(self.__sessnum): 
            for j in range(self.__pathnum[i]):
                for k in range(len(self.__sesspaths[i][j])-1):
                    enode1 = self.__sesspaths[i][j][k]
                    enode2 = self.__sesspaths[i][j][k+1]
                    self.__flowmap[enode1][enode2] += subrates[i][j]
    
    def getUtil(self):
        sesspathutil = []
        for i in range(self.__sessnum): 
            sesspathutil.append([])
            for j in range(self.__pathnum[i]):
                pathutil = []
                for k in range(len(self.__sesspaths[i][j])-1):
                    enode1 = self.__sesspaths[i][j][k]
                    enode2 = self.__sesspaths[i][j][k+1]
                    pathutil.append(round(self.__flowmap[enode1][enode2]/self.__capaMatrix[enode1][enode2], 4))
                sesspathutil[i].append(pathutil)
        netutil = []
        for i in range(self.__nodenum):
            for j in range(self.__nodenum):
                if self.__edgemap[i][j] != 0:
                    netutil.append(round(self.__flowmap[i][j]/self.__capaMatrix[i][j], 4))
        maxutil = max(netutil)
        return round(maxutil, 4), sesspathutil, netutil

    def update(self, action):
        if self.__updatenum % self.__epoch == 0 and self.__updatenum >= 0:
            self.__episode += 1
            self.getRates()
            print("sessrates:")
            print(self.__sessrate)
        
        # testing the broken link edge
        #self.updateCapaMatrix()
        
        self.getFlowMap(action)
        maxutil, sesspathutil, netutil = self.getUtil()
        self.__updatenum += 1
        return maxutil, sesspathutil, netutil

    def getRates(self):
        #self.__sessrate = list(np.array(self.__sessrates[self.__episode + self.__start_index]) / 2.0)
        rate_num = len(self.__sessrates)
        self.__sessrate = self.__sessrates[(self.__episode + self.__start_index) % rate_num]
    
    def getInfo(self):
        return self.__nodenum, self.__sessnum, self.__edgenum, self.__pathnum, self.__sesspaths, self.__edgemap

    def showInfo(self):
        print("--------------------------")
        print("----detail information----")
        print("filepath:%s" % self.__filepath)
        print("nodenum:%d" % self.__nodenum)
        print("sessnum:%d" % self.__sessnum)
        print("pathnum:")
        print(self.__pathnum)
        #print("sessrate:")
        #print(self.__sessrate)
        #print("sesspaths:")
        #print(self.__sesspaths)
        #print("flowmap:")
        #print(self.__flowmap)
        print("--------------------------")
    
    def updateCapaMatrix(self):
        decay_rates = [1, 0.9, 0.7, 0.5, 0.3, 0.1]
        Abi_link_edges = [(1, 4), (1, 5), (1, 11), (2, 5), (2, 8), (3, 6), (3, 9), (3, 10), (4, 6), (4, 7)]
        GEA_link_edges = [(10, 3), (3, 14), (5, 8), (13, 19), (3, 21), (14, 13), (13, 17), (1, 7), (1, 16), (20, 17)]
        if self.__updatenum % 10 == 0:
            if self.__toponame == "GEA":
                self.__capaMatrix[GEA_link_edges[self.__link_ind][0] - 1][GEA_link_edges[self.__link_ind][1] - 1] /= decay_rates[self.__rate_ind]
            elif self.__toponame == "Abi":
                self.__capaMatrix[Abi_link_edges[self.__link_ind][0] - 1][Abi_link_edges[self.__link_ind][1] - 1] /= decay_rates[self.__rate_ind]
            self.__rate_ind = self.__updatenum // 100
            self.__link_ind = (self.__updatenum % 100) // 10
            if self.__toponame == "GEA":
                self.__capaMatrix[GEA_link_edges[self.__link_ind][0] - 1][GEA_link_edges[self.__link_ind][1] - 1] *= decay_rates[self.__rate_ind]
            elif self.__toponame == "Abi":
                self.__capaMatrix[Abi_link_edges[self.__link_ind][0] - 1][Abi_link_edges[self.__link_ind][1] - 1] *= decay_rates[self.__rate_ind]
    
