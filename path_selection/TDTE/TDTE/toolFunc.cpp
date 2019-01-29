#include "toolFunc.h"

double random(double start, double end)
{
	return start + (end - start)*rand() / (RAND_MAX + 1.0);
}
string intToString(int d)
{
	std::ostringstream os;
	if (os << d) return os.str();
	return "Error";
}
template <typename T>
void printv(vector<T> v) {
	for (vector<T>::iterator iter = v.begin(); iter != v.end(); iter++)
	{
		cout << *iter << ' ';
	}
	cout << endl;
}
void printv(vector<uint16> v)
{
	vector<uint16>::iterator iter;
	for (iter = v.begin(); iter != v.end(); iter++) {
		cout << *iter << ' ';
	}
	cout << endl;
}
void printv(vector<int> v)
{
	vector<int>::iterator iter;
	for (iter = v.begin(); iter != v.end(); iter++) {
		cout << *iter << ' ';
	}
	cout << endl;
}
void printv(vector<uint32> v)
{
	vector<uint32>::iterator iter;
	for (iter = v.begin(); iter != v.end(); iter++) {
		cout << *iter << ' ';
	}
	cout << endl;
}
void printv(vector<float> v)
{
	vector<float>::iterator iter;
	for (iter = v.begin(); iter != v.end(); iter++) {
		cout << *iter << ' ';
	}
	cout << endl;
}
void printv(vector<double> v)
{
	vector<double>::iterator iter;
	for (iter = v.begin(); iter != v.end(); iter++) {
		cout << *iter << ' ';
	}
	cout << endl;
}
void printv(vector<string> v)
{
	vector<string>::iterator iter;
	for (iter = v.begin(); iter != v.end(); iter++) {
		cout << (*iter).c_str() << ' ';
	}
	cout << endl;
}
void printv(vector<Session> v)
{
	vector<Session>::iterator iter;
	for (iter = v.begin(); iter != v.end(); iter++) {
		cout << (*iter).first << ' ' <<(*iter).second << endl;
	}
	cout << endl;
}
int cmpv(vector<int> v1, vector<int> v2) {
	int len1 = v1.size();
	int len2 = v2.size();
	if (len1 != len2) return 1;
	for (int i = 0; i < len1; i++) {
		if (v1[i] != v2[i])
			return 1;
	}
	return 0;
}
int findv(vector<vector<int>> V, vector<int> v) {
	int len1 = V.size();
	if (len1 == 0) return -1;
	for (int i = 0; i < len1; i++) {
		if (cmpv(V[i], v) == 0)
			return i;
	}
	return -1;
}