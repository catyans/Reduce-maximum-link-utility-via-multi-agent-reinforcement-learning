#ifndef TOOLFUNC

#define TOOLFUNC

#include <iostream>
#include <vector>
#include <sstream>

using namespace std;

#define MAXWEIGHT 1000000

typedef unsigned long uint32;
typedef unsigned short uint16;//65535
typedef pair<uint16, uint16> Session;

double random(double start, double end);
string intToString(int d);
template <typename T>
void printv(vector<T> v);
void printv(vector<uint16> v);
void printv(vector<uint32> v);
void printv(vector<int> v);
void printv(vector<float> v);
void printv(vector<double> v);
void printv(vector<string> v);
void printv(vector<Session> v);
int cmpv(vector<uint16> v1, vector<uint16> v2);
int findv(vector<vector<int>> V, vector<int> v);

#endif
