from __future__ import division
from gurobipy import *


'''
@parameters:
    nodeNum(int): node number of graph
    sessNum(int): total session number of the nodes
    totalPathNum(total path num): the total Path Number of graph
    oblPaths(shape: [[[int, int, ...], ...],...]): the obl paths 
    pathRates(shape: [int, int,...]): the session rates for each path
    cMatrix(shape:[[int, int, ...], ...]): the capacity matrix
'''
def sorsolver(nodeNum, sessNum, totalPathNum, oblPaths, pathRates, cMatrix):
    model = Model('netflow')

    pathVarNum = totalPathNum
    Maps = [[[] for i in range(nodeNum)] for i in range(nodeNum)]
    pathVarID = 0
    for i in range(sessNum):
        for j in range(len(oblPaths[i])):
            pathlen = len(oblPaths[i][j])
            for k in range(pathlen - 1):
                Maps[oblPaths[i][j][k]][oblPaths[i][j][k + 1]].append(pathVarID)
            pathVarID += 1

    sum0 = 0
    sum1 = 0
    sum2 = 0
    fpath = []
    for i in range(pathVarNum):
        fpath.append(model.addVar(0, GRB.INFINITY, 0, GRB.CONTINUOUS, "path"))
    phi = model.addVar(0, GRB.INFINITY, 0, GRB.CONTINUOUS, "phi")

    pathVarID = 0
    for h in range(sessNum):
        sum0 = 0
        sum1 = 0
        for k in range(len(oblPaths[h])):
            sum0 += fpath[pathVarID]
            sum1 = fpath[pathVarID]
            pathVarID += 1
            model.addConstr(sum1 >= 0)
        model.addConstr(sum0 == 1)
    
    for i in range(nodeNum):
        for j in range(nodeNum):
            tmp = len(Maps[i][j])
            if tmp == 0:
                continue
            sum2 = 0
            for k in range(tmp):
                sum2 += fpath[Maps[i][j][k]] * pathRates[Maps[i][j][k]]
            
            model.addConstr(sum2 <= phi*cMatrix[i][j])

    model.setObjective(phi, GRB.MINIMIZE)
    model.optimize()
    ratios = []
    if model.status == GRB.Status.OPTIMAL:
        
        pathVarID = 0
        for h in range(sessNum):
            for k in range(len(oblPaths[h])):
                splitRatio = fpath[pathVarID].getAttr(GRB.Attr.X)
                ratios.append(splitRatio)
                pathVarID += 1
                
    return ratios
