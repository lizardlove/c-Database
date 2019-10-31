#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include "table.h"

using namespace bpt;
using namespace std;

#define ll long long


Table* Table::instance = NULL;
ll Table::colLen = 100;

Table::Table() {
    vector<ll> colArr;
    for (ll i = 0; i < colLen; i++) {
        colArr.push_back(i+1);
    }
    createTable("zxy", colArr);
}
Table::~Table() {
    lseek(mFlag, 0, SEEK_SET);
    write(mFlag, &mutex, 1);
    saveSource();
}
Table* Table::getInstance() {
    if (instance == NULL) {
        instance = new Table;
    }

    atexit(Table::onProcessExit);
    return instance;
}

int Table::saveSource() {
    if (mutex == 'F') {
        return -1;
    }

    ll oldL = data.size(), newL = dataCp.size();
    if (newL != oldL) {
        lseek(mFlag, 1, SEEK_SET);
        write(mFlag, &count, 8);
        lseek(mFlag, 8*colLen*(oldL+1), SEEK_CUR);
        for (ll i = oldL; i < newL; i++) {
            vector<ll> line = dataCp[i];
            for (ll j = 0; j < colLen; j++) {
                write(mFlag, &line[j], 8);
            }
        }
        data = dataCp;
        lseek(mFlag, 0, SEEK_SET);
    } else {
        return -1;
    }

    return 0;
}

int Table::readSource() {

    mFlag = open(name.data(), O_RDWR);
    if (mFlag == -1) {
        return -1;
    }
    //读取配置信息
    read(mFlag, &mutex, 1);
    if (mutex == 'T') {
        char f = 'F';
        lseek(mFlag, 0, SEEK_SET);
        write(mFlag, &f, 1);
    }
    read(mFlag, &count, 8);

    //读取列属性信息
    for (ll i = 0; i < colLen; i++) {
        ll col;
        read(mFlag, &col, 8);
        colName.push_back(col);
    }

    //读取行数据
    for (ll i = 0; i < count; i++) {
        vector<ll> line;
        for (ll j = 0; j < colLen; j++) {
            ll el;
            read(mFlag, &el, 8);
            line.push_back(el);
        }
        data.push_back(line);
    }

    dataCp = data;

    lseek(mFlag, 0, SEEK_SET);

    return 1;

}

int Table::createTable(const char *table_name, vector<ll> col_name){

    mFlag = open(table_name, O_RDWR);
    if (mFlag == -1) {
        mFlag = open(table_name, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IXUSR);
        colName = col_name;
        name = table_name;
        mutex = 'T';
        count = 0;

        write(mFlag, &mutex, 1);
        write(mFlag, &count, 8);
        for (ll i = 0; i < colLen; i++) {
            write(mFlag, &colName[i], 8);
        }
        lseek(mFlag, 0 ,SEEK_SET);

        mutex = 'T';
 
    } else {
        name = table_name;
        readSource();

        bool indexExist = false;
        for(ll i = 0; i < colName.size(); i++) {
            char path[24] = { 0 };
            sprintf(path, "./IndexPacket/%lld.bin", colName[i]);
            if (is_file_exist(path)) {
                BPlusTree ptr(path, 0);
                bptBox[i] = &ptr;
                indexExist = true;
            }
        }

    }
    return 1;
}

int Table::append(vector<ll> line) {
    if (mutex == 'F') {
        // std::cout << "Can't append new data, please try again later." << std::endl;
        return -1;
    }
    if (line.size() != colLen) {
        // std::cout << "Error: data column-number wrong." << std::endl;
        return -1;
    }
    count++;
    dataCp.push_back(line);

    for (int i = 0; i < 100; i++) {
        if (bptBox[i]) {
            (*bptBox[i]).insert(line[i], count);
        }
    }
    return 0;
}

void Table::onProcessExit() {
    delete instance;
}

void Table::initIndex(const ll col) {
    char path[24] = { 0 };
    sprintf(path, "./IndexPacket/%lld.bin", col);
    BPlusTree ptr(path, (!is_file_exist(path)));

    if (count != 0) {
        for(ll i = 0; i < dataCp.size(); i++) {
            ptr.insert(dataCp[i][col], i);
        }
    }
    bptBox[col] = &ptr;
}

bool Table::checkIndex(const ll col, BPlusTree *ptr) {
    char path[24] = { 0 };
    sprintf(path, "./IndexPacket/%lld.bin", col);
    if (is_file_exist(path)) {
        BPlusTree bptPtr(path, 0);
        ptr = &bptPtr;
        cout << "index- "<< path << " exist" << endl;
        cout << ptr->meta.leaf_node_num << endl;
        return true;
    }
    return false;
    
}

int Table::search(const ll col, ll min, ll max, vector<ll> &result) {
    bool rangeSearch = !(min == max);
    BPlusTree *ptr;
    bool indexExist = false;
    char path[24] = { 0 };
    sprintf(path, "./IndexPacket/%lld.bin", col);
    if (is_file_exist(path)) {
        BPlusTree bptPtr(path, 0);
        ptr = &bptPtr;
        indexExist = true;
    }
    if (indexExist) { //存在索引表
        std::cout << "exist index" << std::endl;
        cout << ptr->meta.leaf_node_num << endl;
        ll values[MAXRANGE];
        int flag;
        if (rangeSearch) { //范围搜索
            flag = (*ptr).search_range(min, max, values, MAXRANGE);
        } else {  //单例搜索
            std::cout << "单例搜索" << std::endl;
            flag = (*ptr).search(min, values, MAXRANGE);
        }
        if (flag) {
            for (ll i = 0; i < MAXRANGE; i++) {
                if (values[i]) {
                    result.push_back(values[i]);
                }
            }
        }
    } else { //没有索引表，遍历搜索
        std::cout << "doesn't index" << std::endl;
        for (ll i = 0; i < dataCp.size(); i++) {
            if (rangeSearch && (dataCp[i][col] >= min && dataCp[i][col] <= max)) {
                result.push_back(i);
            }
            if (!rangeSearch && dataCp[i][col] == min) {
                result.push_back(i);
            }
        }
    }

    if (result.size() > 0) {
        return 1;
    } else {
        return 0;
    }

}

bool Table::is_file_exist(const char *fileName) {
    int fp;
    fp = open(fileName, O_RDWR);
    
    if (fp == -1) {
        close(fp);
        return 0;
    }
    close(fp);
    return 1;
}
