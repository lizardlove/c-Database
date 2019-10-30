#ifndef TABLE_H
#define TABLE_H
#include <vector>
#include <string>

#include "BPlusTree.h"
#define ll long long
using namespace std;
using namespace bpt;

#define MAXRANGE 10

class Table 
{
    private:
        int mFlag;
        static Table * instance;
        vector<ll> colName;
        vector< vector<ll> > data; //源数据

        Table(const Table&);
        Table& operator=(const Table&);


        Table();
        ~Table();
        /* 退出-删除实例 */
        static void onProcessExit();

        /* 读硬盘数据 */
        int readSource();
        /* 写硬盘数据 */
        int saveSource();

        /* 文件存在则读，不存在则新建 */
        int createTable(const char *table_name, vector<ll> col_name);
        
        bool is_file_exist(const char *fileName);
    
    public:
        string name;
        static ll colLen;
        ll count;
        char mutex;
        vector< vector<ll> > dataCp; //备份数据
        
        BPlusTree* bptBox[100];

        /* 获取单例 */
        static Table * getInstance();

        /* 插入数据 */
        int append(vector<ll> line);

        bool checkIndex(const ll col, BPlusTree *ptr);

        /* 按键值新建索引表 */
        void initIndex(const ll col);
        /* 单项搜索 */
        ll searchRecord(const ll col, ll key);

        /* 范围搜索 */
        int search(const ll col, ll min, ll max, vector<ll> &result);

};

#endif