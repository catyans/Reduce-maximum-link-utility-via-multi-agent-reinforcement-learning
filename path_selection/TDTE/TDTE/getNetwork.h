#ifndef GETNETWORK
#define GETNETWORK
#include <stdlib.h>
#include<fstream>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <math.h>
#include <assert.h>
#include <windows.h>
#include <io.h> 
#include <map>
#include <set>
#include "gurobi_c++.h"
#include "toolFunc.h"
#include "GraphElements.h"
#include "Graph.h"
#include "DijkstraShortestPathAlg.h"
#include "YenTopKShortestPathsAlg.h"

using namespace std;

class Network {
public:
	Network(string filePathPrefix, int fileNum, string topoName , string pathType, int multiPathNum, int weightMode, int increMode);
	~Network();

	void logEnable(bool printEnable);
	void getTopo();
	void getSessions();
	void getPaths();
	void writeFile(double Scale);
	void getOptSolu();
	void setBottleneck(int bnFlag, int failStep, double failDegree);

private:
	/*init variables*/
	bool printEnable;
	/*path vars*/
	string filePathPrefix;
	string topoPath;
	string outputPath;
	string sessFilePath;
	string optSoluPath;
	string optValPath;
	/*basic vars*/
	string topoName;
	int sessionNum;
	string pathType;
	int multiPathNum;
	int weightMode;
	int increMode;
	int bnFlag;
	double failDegree;
	int failStep;
	/*storation variables*/
	int nodeNum;
	int linkNum;
	vector<vector<int>> linkSet;//store links
	vector<vector<int>> wMatrix;//link weight matrix
	vector<vector<int>> cMatrix;//link capa matrix
	vector<Session> sessions;
	vector<int> sendRates;
	vector<vector<int>> soluPaths;//selected paths
	vector<vector<vector<int>>> soluPaths2;
	vector<int> soluPathNum;
	vector<vector<double>> soluPathRatio;
	vector<double> mcfSessRatios;//for write MCF solution files
	vector<double> orSessRatios;//for write oblivious routing solution files
	int maxMultiPathNum;
	/*main functions*/
	void shortest();
	void kshortest();
	void mcf();
	void mcfORL();
	void OR(int type, bool valiFlag);
	vector<double> solverMCF(int sessNum, vector<Session> sess, vector<int> rates);
	vector<double> solverORI(int sessNum, vector<Session> sess, vector<int> rates, bool valiFlag);
	void validateOR(vector<double> flow);
	vector<vector<int>> parsePaths(vector<double> solution);
	vector<int> find_path(int src, int dst, vector<int> onepath, vector<vector<int>> subgraph, vector<vector<double>> ratio);
	vector<vector<int>> selePaths(vector<double> sessRatios, vector<vector<int>> sessPaths);
	vector<int> getRates(int fileNum);
};

#endif
