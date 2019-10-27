#include <iostream>
#include <stdlib.h>
#include <vector>
#include <fstream>
#include <time.h> 
#include "BPlusTree.h"

using namespace bpt;
using namespace std;

int insertRecord(BPlusTree *treePtr,ll key, ll values);
int searchRecord(BPlusTree *treePtr,ll index, ll &return_val);
bool is_file_exist(const char *fileName);

const char *dbFileName = "./db.bin";
BPlusTree *bptPtr;

int main() {
    srand((unsigned)time(NULL));
    BPlusTree test(dbFileName, (!is_file_exist(dbFileName)));
    bptPtr = &test;
    cout <<"ok, start test."<< endl;

    vector<ll> box;
    for (ll i = 0; i < 100 ; i++) {
        ll s = rand() % 1000;
        box.push_back(s);
        insertRecord(bptPtr, s, i);
    }
    ll returnValue[10];
    searchRecord(bptPtr, box[50], returnValue);
    cout << "test data: 50" << "--" << returnValue[0] << endl;

}

int insertRecord(BPlusTree *treePtr,ll index, ll values){
	return (*treePtr).insert(index, values);
}

int searchRecord(BPlusTree *treePtr,ll index, ll *return_val){
	return (*treePtr).search(index, return_val, 10); 
}

bool is_file_exist(const char *fileName)
{
    ifstream ifile(fileName);
  	return ifile.good();
}