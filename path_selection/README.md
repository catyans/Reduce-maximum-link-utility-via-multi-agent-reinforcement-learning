# Path selection module

### Description
"TDTE" is a project of vs2015 used for preparing input files of simulations and experiments. "topoSet" is the directory storing the generated files. 

### How to run
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

### Notes
The module is not humanized now and it will be improved in future. 
