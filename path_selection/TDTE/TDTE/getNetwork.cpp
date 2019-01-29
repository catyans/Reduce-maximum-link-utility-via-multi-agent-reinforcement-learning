#include "getNetwork.h"
using namespace std;

Network::Network(string filePathPrefix, int fileNum, string topoName , string pathType, int multiPathNum, int weightMode, int increMode) {
	/*directory path variable*/
	this->filePathPrefix = filePathPrefix + topoName + "\\";
	string outfilePathPrefix = filePathPrefix + topoName + "\\";
	if (pathType == "SHR") { outfilePathPrefix += "SHR\\"; }
	else if (pathType == "MCF") { outfilePathPrefix += "MCF\\"; }
	else if (pathType == "MCFb") { outfilePathPrefix += "MCFb\\"; }
	else if (pathType == "OBL") { outfilePathPrefix += "OBL\\"; }
	else if (pathType == "OBLb") { pathType = "OBL";  outfilePathPrefix += "OBLb\\"; }
	else if (pathType == "OR" || pathType == "ORI") { outfilePathPrefix += "OR\\"; }
	else { ; }
	if (_access((outfilePathPrefix).c_str(), 0) == -1) {
		cout << (outfilePathPrefix).c_str() << endl << "directory not exists!\n";
		exit(1);
	}
	string TXT = ".txt";
	this->topoPath = filePathPrefix + topoName + TXT;
	this->sessFilePath = this->filePathPrefix + intToString(fileNum) + TXT;
	this->outputPath = outfilePathPrefix + topoName + "_" + pathType + "_" + intToString(multiPathNum) + "_" + intToString(fileNum) + TXT;
	this->optSoluPath = outfilePathPrefix + topoName + "_" + pathType + "_" + intToString(multiPathNum) + "_solu" + intToString(fileNum) + TXT;
	if (increMode == 1)
		this->optValPath = outfilePathPrefix + topoName + "_OPT_" + pathType + "b_" + intToString(multiPathNum) + TXT;
	else
		this->optValPath = outfilePathPrefix + topoName + "_OPT_" + pathType + "_" + intToString(multiPathNum) + TXT;

	/*basic variables*/
	this->topoName = topoName;
	this->pathType = pathType;
	this->multiPathNum = multiPathNum;
	this->weightMode = weightMode;
	this->increMode = increMode;
	this->bnFlag = 0;
	this->failStep = 1;
	this->failDegree = 0.5;
	maxMultiPathNum = 0;
	printEnable = false;
}

Network::~Network() {
	if(printEnable) cout << "Network generation finished!" << endl << endl;
}

void Network::logEnable(bool printEnable) {
	this->printEnable = printEnable;
}

void Network::setBottleneck(int bnFlag, int failStep,  double failDegree) {
	this->bnFlag = bnFlag;
	this->failStep = failStep;
	this->failDegree = failDegree;
}

void Network::getTopo() {
	cout << "getTopo()" << endl;
	fstream f;
	f.open(topoPath, ios::in);
	assert(f.is_open());
	char line[200];
	int lineNum = 0;
	f.getline(line, sizeof(line));
	sscanf_s(line, "%d %d", &nodeNum, &linkNum);
	if(printEnable) cout << "nodeNum:" << nodeNum << " linkNum:" << linkNum << endl;
	lineNum++;
	int adj1, adj2, weight, capa;
	for (int i = 0; i < nodeNum; i++) {
		vector<int> wRow;
		vector<int> cRow;
		for (int j = 0; j < nodeNum; j++) {
			if(i ==j ) wRow.push_back(0);
			else wRow.push_back(MAXWEIGHT);
			cRow.push_back(0);
		}
		wMatrix.push_back(wRow);
		cMatrix.push_back(cRow);
	}
	while (!f.eof())
	{
		f.getline(line, sizeof(line));
		sscanf_s(line, "%d %d %d %d", &adj1, &adj2, &weight, &capa);
		if (weightMode == 1) {
			weight = 10; // based on hop number
		}
		if (bnFlag == 1) {
			if ((lineNum - 1) == failStep)
				capa = int(capa*failDegree);
		}
		//capa *= 10;
		wMatrix[adj1 - 1][adj2 - 1] = weight;
		wMatrix[adj2 - 1][adj1 - 1] = weight;
		cMatrix[adj1 - 1][adj2 - 1] = capa;
		cMatrix[adj2 - 1][adj1 - 1] = capa;
		vector<int> link;
		link.push_back(adj1-1); // node id start from 1, not 0!
		link.push_back(adj2-1);
		link.push_back(weight);
		link.push_back(capa);
		linkSet.push_back(link);
		lineNum++;
	}
	f.close();
}

void Network::getSessions()
{
	cout << "getSessions()" << endl;
	fstream f;
	f.open(sessFilePath, ios::in);
	cout << sessFilePath.c_str() << endl;
	assert(f.is_open());
	char line[100];
	int lineNum = 0;
	int src, dst, rate;
	while (!f.eof())
	{
		f.getline(line, sizeof(line));
		sscanf_s(line, "%d %d %d", &src, &dst, &rate);
		Session sess;
		sess.first = src;
		sess.second = dst;
		sessions.push_back(sess);
		///rate *= 10;
		sendRates.push_back(rate);
		lineNum++;
	}
	this->sessionNum = lineNum;
	cout << "linenum:" << lineNum << endl;
	f.close();
}

vector<int> Network::getRates(int fileNum)
{
	cout << "getRates()" << endl;
	fstream f;
	string sessFilePath = filePathPrefix + intToString(fileNum) + ".txt";
	f.open(sessFilePath, ios::in);
	cout << sessFilePath.c_str() << endl;
	assert(f.is_open());
	char line[100];
	int lineNum = 0;
	int src, dst, rate;
	vector<int> rates;
	while (!f.eof())
	{
		f.getline(line, sizeof(line));
		sscanf_s(line, "%d %d %d", &src, &dst, &rate);
		rates.push_back(rate);
		lineNum++;
	}
	f.close();
	return rates;
}

void Network::getPaths() {
	cout << "getPaths(): ";
	if (strcmp(pathType.c_str(), "SHR") == 0 && multiPathNum == 1)
		shortest();
	else if (strcmp(pathType.c_str(), "SHR") == 0)
		kshortest();
	else if (strcmp(pathType.c_str(), "MCF") == 0)
		mcf();
	else if (strcmp(pathType.c_str(), "OBL") == 0)
		mcfORL();
	else if (strcmp(pathType.c_str(), "OR") == 0)
		OR(0, false); // rates are limited
	else if (strcmp(pathType.c_str(), "ORI") == 0)
		OR(1, false); // rates are not limited, i.e. from 0 to infinity
	else
		assert("Path type not defined!");
}

void Network::shortest() {
	cout << "shortest()" << endl;
	vector<vector<int>> weight, R;
	int i, j, k;
	for (i = 0; i < nodeNum; i++) {
		weight.push_back(wMatrix[i]);
		vector<int> tmp;
		for (j = 0; j < nodeNum; j++) {
			tmp.push_back(j);
		}
		R.push_back(tmp);
	}
	for (i = 0; i < nodeNum; i++) {
		for (j = 0; j < nodeNum; j++) {
			for (k = 0; k < nodeNum; k++) {
				if (weight[j][k] > weight[j][i] + weight[i][k]) {
					weight[j][k] = weight[j][i] + weight[i][k];
					R[j][k] = R[j][i];
				}
			}
		}
	}
	vector<vector<int>> solupaths;
	for (i = 0; i < sessionNum; i++) {
		int src = (int)sessions[i].first;
		int dst = (int)sessions[i].second;
		solupaths.clear();
		vector<int> onePath;
		onePath.push_back(src);
		int mid = src;
		while (R[mid][dst] != dst) {
			onePath.push_back(R[mid][dst]);
			mid = R[mid][dst];
		}
		onePath.push_back(dst);
		soluPaths.push_back(onePath);
		solupaths.push_back(onePath);
		soluPaths2.push_back(solupaths);
		soluPathNum.push_back(1);
	}
}

void Network::kshortest() {
	cout << "kshortest()" << endl;
	vector<pair<int,int>> dirEdge;
	vector<double> edgeWeight;
	int i, j;
	pair<int, int> edge;
	for (i = 0; i < linkNum; i++) {
		edge.first = (int)linkSet[i][0];
		edge.second = (int)linkSet[i][1];
		dirEdge.push_back(edge);
		edge.first = (int)linkSet[i][1];
		edge.second = (int)linkSet[i][0];
		dirEdge.push_back(edge);
		edgeWeight.push_back((double)linkSet[i][2]);
		edgeWeight.push_back((double)linkSet[i][2]);
	}
	Graph my_graph(nodeNum, dirEdge, edgeWeight);
	vector<vector<int>> solupaths;
	for (i = 0; i < sessionNum; i++) {
		int src = (int)sessions[i].first;
		int dst = (int)sessions[i].second;
		YenTopKShortestPathsAlg yenAlg(my_graph, my_graph.get_vertex(src), my_graph.get_vertex(dst));
		j = 0;
		solupaths.clear();
		while (yenAlg.has_next()) {
			vector<int> onePathofK = yenAlg.next()->getOnePathofK(cout, printEnable);
			soluPaths.push_back(onePathofK);
			solupaths.push_back(onePathofK);
			++j;
			if (j >= multiPathNum) break;
		}
		soluPaths2.push_back(solupaths);
		soluPathNum.push_back(j);
	}
}

void Network::mcf() {
	cout << "mcf()" << endl;
	//solution: [snode,tnode,u,v,weight,......]
	vector<double> solution = solverMCF(sessionNum, sessions, sendRates);
	if (false) printv(solution);
	soluPaths = parsePaths(solution);
	if (multiPathNum >= 100) {
		int len = mcfSessRatios.size();
		int i;
		ofstream ff(optSoluPath);

		for (i = 0; i < len; i++) {
			ff << mcfSessRatios[i] << endl;
		}
		ff.close();
		cout << "write mcf solution over" << endl;
	}
}

void Network::mcfORL() {
	cout << "mcfOR()" << endl;
	int TMbegin = 100; // 100
	int TMnum = 400; // 400
	if (topoName == "1755") {
		TMbegin = 0;
		TMnum = 30;
	}
	if (topoName == "GEA") {
		TMbegin = 672;
		TMnum = 972;
	}
	if (topoName == "Abi") {
		TMbegin = 0;
		TMnum = 300;
	}
	if (topoName == "EXP") {
		TMbegin = 0;
		TMnum = 10;
	}
	vector<vector<vector<int>>> candidates;
	vector<vector<int>> candiCounts;
	vector<vector<double>> candiWeights;
	vector<vector<int>> tmpCandi;
	vector<int> tmpCount;
	vector<double> tmpWeight;
	for (int i = 0; i < sessionNum; i++) {
		candidates.push_back(tmpCandi);
		candiCounts.push_back(tmpCount);
		candiWeights.push_back(tmpWeight);
	}

	for (int i = TMbegin; i < TMnum; i++) {
		cout << "------>>>>>>>>>>" << i << endl;
		vector<int> sendrates;
		sendrates = getRates(i);
		vector<double> solution = solverMCF(sessionNum, sessions, sendrates);
		soluPaths2.clear();
		soluPathNum.clear();
		mcfSessRatios.clear();
		soluPathRatio.clear();
		soluPaths = parsePaths(solution);//return value not used
		cout << "solution size:" << solution.size() << endl;
		cout << "soluPaths size:" << soluPaths.size() << endl;
		cout << "soluPathNum size:" << soluPathNum.size() << endl;
		for (int j = 0; j < sessionNum; j++) {
			//store paths and count
			int pathNum = soluPathNum[j];
			for (int k = 0; k < pathNum; k++) {
				int res = findv(candidates[j], soluPaths2[j][k]);
				if (res == -1) {
					candidates[j].push_back(soluPaths2[j][k]);
					candiCounts[j].push_back(1);
					candiWeights[j].push_back(soluPathRatio[j][k]);
				}
				else {
					candiCounts[j][res] += 1;
					candiWeights[j][res] += soluPathRatio[j][k];
				}
			}
		}
	}
	soluPaths2.clear();
	soluPathNum.clear();
	soluPathRatio.clear();
	//get top 3 paths for each session; and store the result in soluPaths2 adn soluPathNum
	cout << "begin selecting --->>>>>>>>>>>>>" << endl;
	// small src filter
	int* smallSrc;
	int smallSrcNum = 0;
	int smallSrcGEA[] = { 5, 21, 12, 2, 11, 15, 17, 16, 22, 14, 3 }; /// for GEA [19, 18, 20, 7, 8, 4, 13, 1, 0, 10, 9, 6, 5, 21, 12, 2, 11, 15, 17, 16, 22, 14, 3]
	int smallSrcAbi[] = { 9, 0, 3, 8, 5 }; ///{10, 7, 6, 4, 2, 9, 0, 1, 3, 8, 5};
	if (increMode == 1) {
		if (topoName == "GEA") {
			smallSrc = smallSrcGEA;
			smallSrcNum = 11;
		}
		if (topoName == "Abi") {
			smallSrc = smallSrcAbi;
			smallSrcNum = 5;
		}
	}
	// small src filter end
	int multipathnum = multiPathNum;
	bool smallsrcflag = false;
	for (int i = 0; i < sessionNum; i++) {
		// big source begin 2018.10.15 night
		smallsrcflag = false;
		for (int k = 0; k < smallSrcNum; k++) {
			if (candidates[i][0][0] == smallSrc[k]) {
				smallsrcflag = true;
				break;
			}
		}
		if (smallsrcflag) multipathnum = 1;
		else multipathnum = multiPathNum;
		//big source end

		int pathNum = candidates[i].size();
		if (pathNum <= multipathnum) {
			soluPaths2.push_back(candidates[i]);
			soluPathNum.push_back(pathNum);
			continue;
		}
		tmpCandi.clear();
		int maxCount = -1;
		double maxWeight = -1.0;
		int maxCountId = -1;
		int maxWeightId = -1;
		double cumlWeight = 0.0;
		int j = 0;
		for (j = 0; j < multipathnum; j++) {
			maxCount = -1;
			maxWeight = -1.0;
			maxCountId = -1;
			maxWeightId = -1;
			for (int k = 0; k < pathNum; k++) {
				if (candiCounts[i][k] > maxCount) {
					maxCount = candiCounts[i][k];
					maxCountId = k;
				}
				if (candiWeights[i][k] > maxWeight) {
					maxWeight = candiWeights[i][k];
					maxWeightId = k;
				}
			}

			tmpCandi.push_back(candidates[i][maxWeightId]);
			candiWeights[i][maxWeightId] = 0;

			cumlWeight += maxWeight;

		}
		cout << cumlWeight << " " << cumlWeight/300 << endl;
		soluPaths2.push_back(tmpCandi);
		soluPathNum.push_back(j);
	}
}

void Network::OR(int type, bool valiFlag) {
	cout << "OR()" << endl;
	/*get solution*/
	vector<double> solution; // solution: [snode,tnode,u,v,weight,......]
	if (type == 0) {
		solution = solverORI(sessionNum, sessions, sendRates, false);
	}
	if (type == 1)
		solution = solverORI(sessionNum, sessions, sendRates, valiFlag);
	if (false) printv(solution);

	/*parse solution*/
	soluPaths = parsePaths(solution);
	/*write solution*/
	if (multiPathNum >= 100) {
		orSessRatios = mcfSessRatios;
		int len = orSessRatios.size();
		int i;
		ofstream ff(optSoluPath);

		for (i = 0; i < len; i++) {
			ff << orSessRatios[i] << endl;
		}
		ff.close();
		cout << "write OR solution over" << endl;
	}
}

#if 1
void Network::validateOR(vector<double> flow) {
	cout << "validateOR()" << endl;
	printv(flow);
	cout << flow.size() << endl;
	int i, j, k, l;
	int minRate = 0;
	int sessNum = sessionNum;
	vector<Session> sess = sessions;
	vector<int> rates = sendRates;
	double max_r = 0;
	for (l = 0; l < linkNum; l++) {
		try {
			/*step 1:*/
			cout << "create variables" << endl;
			GRBEnv* env = 0;
			env = new GRBEnv();
			GRBModel model = GRBModel(*env);
			model.set(GRB_StringAttr_ModelName, "netflow");
			GRBVar phi = model.addVar(0, GRB_INFINITY, 0, GRB_CONTINUOUS, "phi"); // objective

			int dVarNum = sessNum;
			int gVarNum = nodeNum * nodeNum * linkNum * 2;
			GRBVar* dab = model.addVars(0, 0, 0, 0, 0, dVarNum);
			GRBVar* gab = model.addVars(0, 0, 0, 0, 0, gVarNum);

			/*step 2:*/
			cout << "add constraints" << endl;
			/*cons 0*/
			//cout << "cons 0" << endl;
			GRBLinExpr sum = 0;
			for (i = 0; i < sessNum; i++) {
				int varId1, varId2;
				varId1 = l * nodeNum*nodeNum + sess[i].first*nodeNum + sess[i].second;
				varId2 = l * nodeNum*nodeNum + sess[i].first*nodeNum + sess[i].second + nodeNum*nodeNum*linkNum;
				sum += (flow[varId1] + flow[varId2]) * dab[i] / linkSet[l][3];
			}
			model.addConstr(sum == phi);

			/*cons 1*/
			//cout << "cons 1" << endl;
			for (i = 0; i < sessNum; i++) {
				model.addConstr(dab[i] >= minRate);
				///model.addConstr(dab[i] <= maxRate);
			}

			/*cons 2*/ // "add conservation constraints"
			//cout << "cons 2" << endl;
			int m, n;
			GRBLinExpr sumin = 0;
			GRBLinExpr sumout = 0;
			for (i = 0; i < sessNum; i++) {
				//break;
				for (j = 0; j < nodeNum; j++) {
					m = sess[i].first;
					n = sess[i].second;
					sumin = 0;
					sumout = 0;
					for (k = 0; k < linkNum; k++) {
						int varId = k*nodeNum*nodeNum + m*nodeNum + n;
						if (j == linkSet[k][0]) {
							sumin += gab[varId];
							sumout += gab[varId + nodeNum*nodeNum*linkNum];
						}
						if (j == linkSet[k][1]) {
							sumin += gab[varId + nodeNum*nodeNum*linkNum];
							sumout += gab[varId];
						}
					}
					if (m == j) model.addConstr(sumin - sumout == dab[i]);
					else if (n == j) model.addConstr(sumin - sumout == -dab[i]);
					else model.addConstr(sumin - sumout == 0);
				}
			}

			/*cons 3*/
			//cout << "cons 3" << endl;
			GRBLinExpr sum1 = 0;
			GRBLinExpr sum2 = 0;
			for (i = 0; i < linkNum; i++) {
				sum1 = 0;
				sum2 = 0;
				for (j = 0; j < sessNum; j++) {
					sum1 += gab[i*nodeNum*nodeNum + sess[j].first*nodeNum + sess[j].second];
					sum2 += gab[i*nodeNum*nodeNum + sess[j].first*nodeNum + sess[j].second + nodeNum*nodeNum*linkNum];
				}
				model.addConstr(sum1 + sum2 <= linkSet[l][3]);
				//model.addConstr(sum2 <= linkSet[l][3]);
			}
			/*================*/

			model.setObjective((GRBLinExpr)phi, GRB_MAXIMIZE);
			model.optimize();
			if (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL) {
				cout << "\nOPTCost: " << model.get(GRB_DoubleAttr_ObjVal) << endl;
				double OptimalCost = model.get(GRB_DoubleAttr_ObjVal);
				if (OptimalCost > max_r) max_r = OptimalCost;
			}
			else {
				cout << "No solution" << endl;
			}
		}
		catch (GRBException e) {
			cout << "Error code =" << e.getErrorCode() << endl;
			cout << e.getMessage() << endl;
		}
		catch (...) {
			cout << "Exception during optimization" << endl;
		}
	}
	cout << "max_r: " << max_r << endl;
}
#endif

#if 1 
vector<double> Network::solverORI(int sessNum, vector<Session> sess, vector<int> rates, bool valiFlag) {
	cout << "solverORUlti()" << endl;

	int i, j, k, l;
	vector<double> solution;
	cout << "prepare for constraints" << endl;
	vector<int> sSet, tSet, rSet;
	for (i = 0; i < sessNum; i++) {
		sSet.push_back(sess[i].first);
		tSet.push_back(sess[i].second);
		rSet.push_back(rates[i]);
	}
	try {
		/*step 1:*/
		cout << "create variables" << endl;
		GRBEnv* env = new GRBEnv();
		GRBModel model = GRBModel(*env);
		model.set(GRB_StringAttr_ModelName, "oblrouting");
		GRBVar r = model.addVar(0, GRB_INFINITY, 0, GRB_CONTINUOUS, "r");; // objective

		int piVarNum = linkNum * linkNum;
		int flowVarNum = nodeNum * nodeNum * linkNum * 2;
		int pVarNum = nodeNum * nodeNum * linkNum;
		
		GRBVar* pi = model.addVars(0, 0, 0, 0, 0, piVarNum);
		GRBVar* flow = model.addVars(0, 0, 0, 0, 0, flowVarNum);
		GRBVar* p = model.addVars(0, 0, 0, 0, 0, pVarNum);

		/*step 2:*/
		cout << "add constraints" << endl;
		/*cons 0*/ // "add conservation constraints"
		int src, dst, s;
		int varId;
		GRBLinExpr sumin = 0;
		GRBLinExpr sumout = 0;
		for (src = 0; src < nodeNum; src++) {
			for (dst = 0; dst < nodeNum; dst++) {
				if (src == dst) continue;
				for (i = 0; i < nodeNum; i++) {
					sumin = 0;
					sumout = 0;
					for (l = 0; l < linkNum; l++) {
						varId = l*nodeNum*nodeNum + src*nodeNum + dst;
						if (i == linkSet[l][1]) {
							sumin += flow[varId];
							sumout += flow[varId + nodeNum*nodeNum*linkNum];
						}
						if (i == linkSet[l][0]) {
							sumin += flow[varId + nodeNum*nodeNum*linkNum];
							sumout += flow[varId];
						}
					}
					if (src == i) model.addConstr(sumout - sumin == 1);
					else if (dst == i) model.addConstr(sumout - sumin == -1);
					else model.addConstr(sumout - sumin == 0);
				}
			}
		}
		/*cons 1*/
		GRBLinExpr sum = 0;
		int l1, l2;
		for (l1 = 0; l1 < linkNum; l1++) {
			sum = 0;
			for (l2 = 0; l2 < linkNum; l2++) {
				sum += linkSet[l2][3] * pi[l1*linkNum + l2]; //linkSet[l2][3] is the capacity of link l2
			}
			model.addConstr(sum <= r);
		}
		/*cons 2*/
		for (l = 0; l < linkNum; l++) {
			for (i = 0; i < nodeNum; i++) {
				for (j = 0; j < nodeNum; j++) {
					if (i == j) continue;
					varId = l*nodeNum*nodeNum + i*nodeNum + j;
					model.addConstr((flow[varId] + flow[varId + nodeNum*nodeNum*linkNum]) / linkSet[l][3] <= p[varId]);
				}
			}
		}
		/*cons 3*/
		int e;
		for (l = 0; l < linkNum; l++) {
			for (e = 0; e < linkNum; e++) {
				for (i = 0; i < nodeNum; i++) {
					model.addConstr(pi[l*linkNum + e] + p[l*nodeNum*nodeNum + i*nodeNum + linkSet[e][0]] - p[l*nodeNum*nodeNum + i*nodeNum + linkSet[e][1]] >= 0);
					model.addConstr(pi[l*linkNum + e] + p[l*nodeNum*nodeNum + i*nodeNum + linkSet[e][1]] - p[l*nodeNum*nodeNum + i*nodeNum + linkSet[e][0]] >= 0);
				}
			}
		}
		/*cons 5*/
		for (l1 = 0; l1 < linkNum; l1++) {
			for (l2 = 0; l2 < linkNum; l2++) {
				model.addConstr(pi[l1*linkNum + l2] >= 0);
			}
		}
		/*cons 6-7*/
		for (l = 0; l < linkNum; l++) {
			for (i = 0; i < nodeNum; i++) {
				model.addConstr(p[l*nodeNum*nodeNum + i*nodeNum + i] == 0);
				for (j = 0; j < nodeNum; j++) {
					model.addConstr(p[l*nodeNum*nodeNum + i*nodeNum + j] >= 0);
				}
			}
		}
		for (varId = 0; varId < flowVarNum; varId++) {
			model.addConstr(flow[varId] <= 1.0);
		}
		/*================*/

		model.setObjective((GRBLinExpr)r, GRB_MINIMIZE);
		model.optimize();
		if (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL) {
			cout << "\nOPTCost: " << model.get(GRB_DoubleAttr_ObjVal) << endl;
			double OptimalCost = model.get(GRB_DoubleAttr_ObjVal);
			/*validate OR solution*/
			vector<double> flowV;
			for (i = 0; i < flowVarNum; i++) {
				flowV.push_back(flow[i].get(GRB_DoubleAttr_X));
			}
			if (valiFlag)
				validateOR(flowV);
			/*get solution*/
			double data_in = 0.0;
			int m, n;
			for (i = 0; i < sessNum; i++) {
				for (j = 0; j < nodeNum; j++) {
					m = sess[i].first;
					n = sess[i].second;
					for (k = 0; k < linkNum; k++) {
						int varId = k*nodeNum*nodeNum + m*nodeNum + n;
						if (j == linkSet[k][0]) {
							sumin += flow[varId];
							sumout += flow[varId + nodeNum*nodeNum*linkNum];
						}
						if (j == linkSet[k][0] || j == linkSet[k][1]) {
							double solu1 = flow[varId].get(GRB_DoubleAttr_X);
							double solu2 = flow[varId + nodeNum*nodeNum*linkNum].get(GRB_DoubleAttr_X);
							if (solu1 <= 0 || solu2 <= 0) {
								if (solu1 > 0) {
									data_in = solu1;
									if (data_in > 0.001) {
										solution.push_back(float(sSet[i]));
										solution.push_back(float(tSet[i]));
										solution.push_back(float(linkSet[k][0]));
										solution.push_back(float(linkSet[k][1]));
										solution.push_back(data_in);
									}
								}
								else {
									data_in = solu2;
									if (data_in > 0.001) {
										solution.push_back(float(sSet[i]));
										solution.push_back(float(tSet[i]));
										solution.push_back(float(linkSet[k][1]));
										solution.push_back(float(linkSet[k][0]));
										solution.push_back(data_in);
									}
								}
								continue;
							}
							if (solu1 > solu2) {
								solu1 -= solu2;
								solu2 = 0;
								data_in = solu1;
								if (data_in > 0.001) {
									solution.push_back(float(sSet[i]));
									solution.push_back(float(tSet[i]));
									solution.push_back(float(linkSet[k][0]));
									solution.push_back(float(linkSet[k][1]));
									solution.push_back(data_in);
								}
							}
							else {
								solu2 -= solu1;
								solu1 = 0;
								data_in = solu2;
								if (data_in > 0.001) {
									solution.push_back(float(sSet[i]));
									solution.push_back(float(tSet[i]));
									solution.push_back(float(linkSet[k][1]));
									solution.push_back(float(linkSet[k][0]));
									solution.push_back(data_in);
								}
							}
						}
					}
				}
			}
			cout << "finish" << endl;
		}
		else {
			cout << "No solution" << endl;
		}
	}
	catch (GRBException e) {
		cout << "Error code =" << e.getErrorCode() << endl;
		cout << e.getMessage() << endl;
	}
	catch (...) {
		cout << "Exception during optimization" << endl;
	}
	return solution;
}
#endif

vector<double> Network::solverMCF(int sessNum, vector<Session> sess, vector<int> rates) {
	cout << "solverMCF()" << endl;
	int i, j, k;
	vector<double> solution;
	cout << "prepare for constraints" << endl;
	vector<double> inflowrow(nodeNum, 0.0);
	vector<vector<double>> inflow(sessNum, inflowrow);
	vector<int> sSet, tSet, rSet;
	///cout << "---" << sessNum << " " << nodeNum << endl;
	for (i = 0; i < sessNum; i++) {
		sSet.push_back(sess[i].first);
		tSet.push_back(sess[i].second);
		rSet.push_back(rates[i]);
		inflow[i][sSet[i]] += (double)rSet[i];
		inflow[i][tSet[i]] -= (double)rSet[i];
	}
	try {
		GRBEnv* env = 0;
		env = new GRBEnv();
		GRBModel model = GRBModel(*env);
		model.set(GRB_IntParam_OutputFlag, 0);
		model.set(GRB_StringAttr_ModelName, "netflow");
		cout << "generate flow variable Maps" << endl;
		int flowVarNum = sessNum * linkNum * 2;
		int flowVarID = 0;
		vector<int> map2(nodeNum, 0);
		vector<vector<int>> map1(nodeNum, map2);
		vector<vector<vector<int>>> Maps(sessNum, map1);//map f_i,u,v to flowVarID

		for (k = 0; k < sessNum; k++)
		{
			for (i = 0; i < linkNum; i++)
			{
				Maps[k][linkSet[i][0]][linkSet[i][1]] = flowVarID;
				flowVarID++;
				Maps[k][linkSet[i][1]][linkSet[i][0]] = flowVarID;
				flowVarID++;
			}
		}
		cout << "add capacaity and objective constraints" << endl;
		GRBVar* flow = 0;
		GRBVar phi;
		GRBLinExpr sum = 0;
		GRBLinExpr sum1 = 0;
		GRBLinExpr sum2 = 0;
		double up = 1.0;
		flow = model.addVars(0, 0, 0, 0, 0, flowVarNum);
		phi = model.addVar(0, GRB_INFINITY, 0, GRB_CONTINUOUS, "phi");
		for (int h = 0; h < linkNum; h++) {
			i = linkSet[h][0];
			j = linkSet[h][1];
			sum1 = 0;
			sum2 = 0;
			for (k = 0; k < sessNum; k++) {
				sum1 += flow[Maps[k][i][j]];
				sum2 += flow[Maps[k][j][i]];
			}
			//for super large traffic amount
			model.addConstr(sum1 <= phi*linkSet[h][3]);
			model.addConstr(sum2 <= phi*linkSet[h][3]);
		}
		cout << "add conservation constraints" << endl;
		GRBLinExpr sumpass = 0;
		GRBLinExpr sumin = 0;
		GRBLinExpr sumout = 0;

		for (k = 0; k< sessNum; k++) {
			for (j = 0; j < nodeNum; j++) {
				sumin = 0;
				sumout = 0;
				for (i = 0; i < nodeNum; i++) {
					if (wMatrix[i][j] < MAXWEIGHT && i != j) {
						sumin += flow[Maps[k][i][j]]; //sum(k,*,j)
						sumout += flow[Maps[k][j][i]]; //sum(k,j,*)
					}
				}
				if (sSet[k] == j) {
					model.addConstr(sumin == 0);
					model.addConstr(sumout == inflow[k][j]);
				}
				else if (tSet[k] == j) {
					model.addConstr(sumout == 0);
					model.addConstr(sumin + inflow[k][j] == 0);
				}
				else {
					model.addConstr(sumin + inflow[k][j] == sumout);
				}
			}
		}

		cout << "set objective and solve the problem" << endl;
		model.setObjective((GRBLinExpr)phi, GRB_MINIMIZE);
		model.optimize();
		if (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL) {
			cout << "\nOPTCost: " << model.get(GRB_DoubleAttr_ObjVal) << endl;
			double OptimalCost = model.get(GRB_DoubleAttr_ObjVal);
			if (pathType == "MCF" || pathType == "MCFb") {
				ofstream fopt(optValPath, ios::app);
				fopt << OptimalCost << endl;
				fopt.close();
			}
			cout << "store the solution" << endl;
			double data_in = 0.0;
			for (k = 0; k < sessNum; k++) {
				for (i = 0; i < nodeNum; i++) {
					for (j = 0; j < i; j++) {
						if (wMatrix[i][j] < MAXWEIGHT)
						{
							double solu1 = flow[Maps[k][i][j]].get(GRB_DoubleAttr_X);
							double solu2 = flow[Maps[k][j][i]].get(GRB_DoubleAttr_X);
							if (solu1 <= 0 || solu2 <= 0) {
								if (solu1 > 0) {
									data_in = float(solu1 / rSet[k]);
									if (data_in > 0.001) {
										solution.push_back(float(sSet[k]));
										solution.push_back(float(tSet[k]));
										solution.push_back(float(i));
										solution.push_back(float(j));
										solution.push_back(data_in);
									}
								}
								else {
									data_in = float(solu2 / rSet[k]);
									if (data_in > 0.001) {
										solution.push_back(float(sSet[k]));
										solution.push_back(float(tSet[k]));
										solution.push_back(float(j));
										solution.push_back(float(i));
										solution.push_back(data_in);
									}
								}
								continue;
							}
							if (solu1 > solu2) {
								solu1 -= solu2;
								solu2 = 0;
								data_in = float(solu1 / rSet[k]);
								if (data_in > 0.001) {
									solution.push_back(float(sSet[k]));
									solution.push_back(float(tSet[k]));
									solution.push_back(float(i));
									solution.push_back(float(j));
									solution.push_back(data_in);
								}
							}
							else {
								solu2 -= solu1;
								solu1 = 0;
								data_in = float(solu2 / rSet[k]);
								if (data_in > 0.001) {
									solution.push_back(float(sSet[k]));
									solution.push_back(float(tSet[k]));
									solution.push_back(float(j));
									solution.push_back(float(i));
									solution.push_back(data_in);
								}
							}
						}
					}
				}
			}
			cout << "finish" << endl;
		}
		else {
			cout << "No solution" << endl;
		}
	}
	catch (GRBException e) {
		cout << "Error code =" << e.getErrorCode() << endl;
		cout << e.getMessage() << endl;
	}
	catch (...) {
		cout << "Exception during optimization" << endl;
	}
	return solution;
}

vector<vector<int>> Network::parsePaths(vector<double> solution) {
	cout << "parsePaths()" << endl;
	int i, j;
	vector<vector<int>> solupaths;
	vector<vector<int>> sessPaths;
	vector<double> sessRatios;

	int count = 0;
	int solulen = solution.size();
	int graphNotMaxNum = 0;
	int s, t, x, y;//snode,tnode,u,v 
	while (count < solulen)//tranverse solution, i.e. each snode,tnode 
	{
		graphNotMaxNum = 0;
		vector<int> subgraphrow(nodeNum, MAXWEIGHT);
		vector<double> ratiorow(nodeNum, 0.0);
		vector<vector<int>> subgraph(nodeNum, subgraphrow);
		vector<vector<double>> ratio(nodeNum, ratiorow);

		s = int(solution[count]);
		t = int(solution[count + 1]);
		while (count < solulen)//
		{
			x = int(solution[count]);
			y = int(solution[count + 1]);
			if (s != x || t != y) break;
			count += 2;
			subgraph[int(solution[count])][int(solution[count + 1])] = 1;
			graphNotMaxNum++;
			ratio[int(solution[count])][int(solution[count + 1])] = solution[count + 2];
			count += 3;
		}
		int pathNum = 0;
		vector<int> onepath;
		sessPaths.clear();
		sessRatios.clear();
		if (printEnable) cout << endl;
		while (graphNotMaxNum > 0)//parse paths of s,t
		{
			onepath.clear();
			onepath = find_path(s, t, onepath, subgraph, ratio);
			if (onepath.empty())break;//parse over
			double minweight = 2.0;
			int pathlen = onepath.size();
			for (i = 0; i < pathlen - 1; i++)
			{
				if (ratio[onepath[i]][onepath[i + 1]] < minweight)
				{
					minweight = ratio[onepath[i]][onepath[i + 1]];
				}
			}
			for (i = 0; i < pathlen - 1; i++)
			{
				ratio[onepath[i]][onepath[i + 1]] -= minweight;
				if (ratio[onepath[i]][onepath[i + 1]] < 0.005)//link load is too slow, ignore
				{
					ratio[onepath[i]][onepath[i + 1]] = 0;
					subgraph[onepath[i]][onepath[i + 1]] = MAXWEIGHT;
					graphNotMaxNum--;
				}
			}
			if (minweight < 0.005) continue;
			pathNum++;
			sessPaths.push_back(onepath);
			
			if (minweight > 1.0) {
				cout << pathlen << endl;
				printv(onepath);
				for (i = 0; i < pathlen - 1; i++)
				{
					cout << ratio[onepath[i]][onepath[i + 1]] << " ";
				}
				for (i = 0; i < nodeNum; i++)
				{
					for (j = 0; j < nodeNum; j++)
					{
						cout << ratio[i][j] << " ";
					}
					cout << endl;
				}
				cout << endl;
				cout << "ERROR: too large minweight:" << minweight << endl;
				exit(1);
			}
			sessRatios.push_back(minweight);
			mcfSessRatios.push_back(minweight);
			if (printEnable) cout << "s" << s << "->" << "t" << t << "(";
			if (printEnable) cout << minweight << "): ";
			if (printEnable) printv(onepath);
			
		}
		vector<vector<int>> seleSessPaths = selePaths(sessRatios, sessPaths);
		int sessPathNum = seleSessPaths.size();
		for (int k = 0; k < sessPathNum; k++) {
			solupaths.push_back(seleSessPaths[k]);
		}
		soluPaths2.push_back(seleSessPaths);
		soluPathNum.push_back(sessPathNum);
	}
	return solupaths;
}

vector<int> Network::find_path(int src, int dst, vector<int> onepath,vector<vector<int>> subgraph, vector<vector<double>> ratio) {
	vector<int> tmp;
	onepath.push_back(src);
	if (src == dst) return onepath;
	int u;
	for (u = 0; u < nodeNum; u++) {
		if (subgraph[src][u] != MAXWEIGHT)break;
	}
	if (u >= nodeNum)return tmp;
	for (u = 0; u < nodeNum; u++) {
		if (subgraph[src][u] != MAXWEIGHT) {
			vector<int>::iterator iter = find(onepath.begin(), onepath.end(), u);
			if (iter == onepath.end()) {
				tmp = find_path(u, dst, onepath, subgraph, ratio);
				if (!tmp.empty())return tmp;
			}
			else {
				;
			}
		}
	}
	return tmp;
}

vector<vector<int>> Network::selePaths(vector<double> sessRatios, vector<vector<int>> sessPaths) {
	int i, j;
	int sessPathNum = sessPaths.size();
	if (sessPathNum > maxMultiPathNum) maxMultiPathNum = sessPathNum;
	if (sessPathNum <= multiPathNum) {
		soluPathRatio.push_back(sessRatios);
		return sessPaths;
	}
	vector<double> soluratios;
	vector<double> sessratios = sessRatios;
	vector<int> ids;
	for (i = 0; i < sessPathNum; i++) {
		ids.push_back(i);
	}
	double ratioTmp;
	int idTmp;
	for (i = 0; i < sessPathNum; i++) {
		for (j = 0; j < sessPathNum - i - 1; j++) {
			if (sessratios[j] < sessratios[j + 1])
			{
				ratioTmp = sessratios[j];
				sessratios[j] = sessratios[j + 1];
				sessratios[j + 1] = ratioTmp;
				idTmp = ids[j];
				ids[j] = ids[j + 1];
				ids[j + 1] = idTmp;
			}
		}
	}
	vector<vector<int>> seleSessPaths;
	for (i = 0; i < multiPathNum; i++) {
		seleSessPaths.push_back(sessPaths[ids[i]]);
		soluratios.push_back(sessratios[i]);
	}
	soluPathRatio.push_back(soluratios);
	for (i = 0; i < multiPathNum; i++) {
		cout << "---------" << endl;
		cout << sessratios[i] << ' ';
		printv(sessPaths[ids[i]]);
	}
	return seleSessPaths;
}

void Network::writeFile(double Scale) {
	double scale = Scale;
	cout << "writeFile()" << endl;

#if 1
	if (multiPathNum > 99)
		multiPathNum = maxMultiPathNum;
	int i, j, k;
	ofstream ff(outputPath);
	ff << (int)(10000/scale) << endl;
	/*add hosts*/
	int hostId = nodeNum;
	vector<vector<int>> nodeHost;
	for (i = 0; i < nodeNum; i++) {
		vector<int> adjHosts;
		nodeHost.push_back(adjHosts);
	}

	int pathLen = 0;
	int src = nodeNum, dst = nodeNum;
	int dstHostIndex = 0;
	int srcHostIndex = 0;
	for (i = 0; i < sessionNum; i++) {
		int pathNum = soluPathNum[i];
		for (k = 0; k < pathNum; k++) {
			pathLen = soluPaths2[i][k].size();
			if (src != soluPaths2[i][k][0] || dst != soluPaths2[i][k][pathLen - 1]) {
				src = soluPaths2[i][k][0];
				dst = soluPaths2[i][k][pathLen - 1];
				dstHostIndex = nodeHost[dst].size();
				srcHostIndex = nodeHost[src].size();

				nodeHost[src].push_back(hostId);
				hostId++;

				int tmp = pathNum;
				while (tmp--) {
					nodeHost[dst].push_back(hostId);
					hostId++;
				}
			}
			ff << nodeHost[src][srcHostIndex] << ',';
			for (j = 0; j < pathLen; j++)
			{
				ff << soluPaths2[i][k][j] << ',';
			}
			ff << nodeHost[dst][dstHostIndex] << endl;
			dstHostIndex++;
		}
	}
	ff << "succeed" << endl;
	for (i = 0; i < sessionNum - 1; i++) {
		ff << sendRates[i]/scale << ',';
	}
	ff << sendRates[i]/scale;
	ff.close();
#endif
	cout << "write txt over" << endl;
}

void Network::getOptSolu() {
	cout << "getOptSolu()" << endl;
	if ((pathType == "MCF" || pathType == "MCFb") && multiPathNum == 100) {
		solverMCF(sessionNum, sessions, sendRates);
	}
	if (pathType == "OBL" || pathType == "SHR" || (pathType == "MCF" && multiPathNum < 100)) {
		int sessNum = sessionNum;
		vector<Session> sess = sessions;
		vector<int> rates = sendRates;
		int i, j, k;

		//get paths
		fstream in;
		in.open(outputPath, ios::in);
		cout << "outputPath:" << outputPath << endl;
		char buffer[500];
		vector<vector<int>> res;
		in.getline(buffer, 500);
		while (!in.eof()) {
			in.getline(buffer, 500);
			if (buffer[0] == 's') break;//"succeed" means the end of paths
			string str(buffer);
			vector<int> vec;
			unsigned int start = 0;
			unsigned int pos = 0;
			do {
				pos = str.find_first_of(',', start);//search the first ',' from start and return the index of ','
				int num = atoi(str.substr(start, pos - start).c_str());
				vec.push_back(num);
				start = pos + 1;
			} while (pos<500);
			res.push_back(vec);
		}
		in.close();
		int sessId = 0;
		vector<vector<vector<int>>> oblPaths;
		vector<vector<int>> oblpaths;
		int totalPathNum = res.size();
		vector<int> pathRates;
		for (i = 0; i < totalPathNum; i++) {
			vector<int> vec;
			int pathlen = res[i].size();
			for (j = 1; j < pathlen - 1; j++) {
				vec.push_back(res[i][j]);
			}
			if (sess[sessId].first != res[i][1] || sess[sessId].second != res[i][pathlen - 2]) {
				oblPaths.push_back(oblpaths);
				sessId += 1;
				oblpaths.clear();
			}
			//printv(vec);
			pathRates.push_back(rates[sessId]);
			oblpaths.push_back(vec);
		}
		oblPaths.push_back(oblpaths);
		for (i = 0; i < sessionNum; i++) {
			break;
			cout << "-----" << i << endl;
			for (j = 0; j < oblPaths[i].size(); j++) {
				printv(oblPaths[i][j]);
			}
		}
		
		try {
			GRBEnv* env = 0;
			env = new GRBEnv();
			GRBModel model = GRBModel(*env);
			model.set(GRB_StringAttr_ModelName, "netflow");
			cout << "generate path variable constraints" << endl;
			int pathVarNum = totalPathNum;
			int pathVarID = 0;
			vector<int> map2;
			vector<vector<int>> map1(nodeNum, map2);
			vector<vector<vector<int>>> Maps(nodeNum, map1);//map u,v to pathVarID

			pathVarID = 0;
			for (i = 0; i < sessionNum; i++) {
				for (j = 0; j < oblPaths[i].size(); j++) {
					int pathlen = oblPaths[i][j].size();
					for (k = 0; k < pathlen - 1; k++) {
						Maps[oblPaths[i][j][k]][oblPaths[i][j][k + 1]].push_back(pathVarID);
					}
					pathVarID += 1;
				}
			}
			cout << "add capacaity and objective constraints" << endl;
			GRBVar* fpath = 0;
			GRBVar phi;
			GRBLinExpr sum = 0;
			GRBLinExpr sum1 = 0;
			GRBLinExpr sum2 = 0;
			fpath = model.addVars(0, 0, 0, 0, 0, pathVarNum);
			phi = model.addVar(0, GRB_INFINITY, 0, GRB_CONTINUOUS, "phi");
			pathVarID = 0;
			for (int h = 0; h < sessionNum; h++) {
				sum = 0;
				sum1 = 0;
				for (k = 0; k < oblPaths[h].size(); k++) {
					sum += fpath[pathVarID];
					sum1 = fpath[pathVarID];
					pathVarID += 1;
					model.addConstr(sum1 >= 0);
				}
				model.addConstr(sum == 1);
			}
			for (i = 0; i < nodeNum; i++) {
				for (j = 0; j < nodeNum; j++) {
					int tmp = Maps[i][j].size();
					if (tmp == 0) continue;
					sum2 = 0;
					for (k = 0; k < tmp; k++) {
						sum2 += fpath[Maps[i][j][k]] * pathRates[Maps[i][j][k]];
					}
					model.addConstr(sum2 <= phi*cMatrix[i][j]);
				}
			}
			cout << "set objective and solve the problem" << endl;
			model.setObjective((GRBQuadExpr)phi, GRB_MINIMIZE);
			model.optimize();
			if (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL) {
				cout << "\nOPTCost: " << model.get(GRB_DoubleAttr_ObjVal) << endl;
				double OptimalCost = model.get(GRB_DoubleAttr_ObjVal);
				ofstream fopt(optValPath, ios::app);
				fopt << OptimalCost << endl;
				fopt.close();
			}
			else {
				cout << "No solution" << endl;
			}
		}
		catch (GRBException e) {
			cout << "Error code =" << e.getErrorCode() << endl;
			cout << e.getMessage() << endl;
		}
		catch (...) {
			cout << "Exception during optimization" << endl;
		}
	}
}