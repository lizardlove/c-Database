
#include <vector>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <time.h> 
#include <pthread.h>
#include "table.h"
using namespace std;

#define NUM_THREADS 5

const char *errorMessage = "> your input is invalid,print \".help\" for more infomation!\n";
const char *nextLineHeader = "> ";
Table* zxyPtr;

clock_t startTime,finishTime;  

void initialSystem();
void printOperatorMessage();
void selectCommand();
void printInfo(Table *tablePtr);
int insertRandomRecord(Table *tablePtr, int num);
int searchRecord(Table* tablePtr, ll col, ll min, ll max, vector<ll> &searchResult);
int checkRecord(Table* tablePtr, ll row,  ll col = -1);
int initIndex(Table* tablePtr, ll col);
double durationTime(clock_t *f,clock_t *s);

void initialSystem() {
    printOperatorMessage();
    zxyPtr = Table::getInstance();
    selectCommand();
}

void printOperatorMessage() {
    cout << " .help                         print the instruction." << endl
         << " .exit                         exit test. " << endl
         << " .info                         print table information" << endl
         << " insert {num};                 insert num random data to table." << endl
         << " search {column} {min} {max};  search record by range." << endl
         << " index {column};               init column's index." << endl
         << " check {row} {column};         check data, column doesn't require;" << endl
         << endl << nextLineHeader;
}

void selectCommand() {
    char *userCommand = new char[256];
    while(true) {
        cin.getline(userCommand, 256);

        if (strcmp(userCommand, ".exit") == 0) {
            break;
        } else if (strcmp(userCommand, ".info") == 0) {
            printInfo(zxyPtr);

        }else if (strcmp(userCommand, ".help") == 0) {
            printOperatorMessage();
        } else if (strncmp(userCommand, "insert", 6) == 0) {
            int num;
            int inputValid = sscanf(userCommand, "insert %d;", &num);
            if (inputValid == 1) {
                startTime = clock();
                int operatorCode = insertRandomRecord(zxyPtr, num);
                finishTime = clock();
                if (operatorCode == 1) {
                    cout << "insert data successful, time: " << durationTime(&finishTime, &startTime) << " seconds\n" << nextLineHeader;
                } else {
                    cout << "> failed!\n" << nextLineHeader;
                }
            } else {
                cout << errorMessage << nextLineHeader;
            }
        } else if (strncmp(userCommand, "search", 6) == 0) {
            ll column, min, max;
            int inputValid = sscanf(userCommand, "search %lld %lld %lld;", &column, &min, &max);
            if (inputValid == 3) {
                startTime = clock();
                vector<ll> searchResult;
                int operatorCode = searchRecord(zxyPtr, column, min, max, searchResult);
                finishTime = clock();
                cout << operatorCode << endl;
                if (operatorCode == 1 && searchResult.size() > 0) {

                    cout << "search successful, time: " << durationTime(&finishTime, &startTime) << " seconds\n";
                    cout << "row" << endl;
                    for (int i = 0; i < searchResult.size(); i++) {
                        cout << searchResult[i] << endl;
                    }
                    cout << "You can use check {row} {column} to check data." << endl << nextLineHeader;
                } else {
                    cout << "> failed!\n" << nextLineHeader;
                }
            } else {
                cout << errorMessage << nextLineHeader;
            }
        } else if (strncmp(userCommand, "check", 5) == 0) {
            ll row, column;
            int inputValid = sscanf(userCommand, "check %lld %lld;", &row, &column);
            if (inputValid == 2) {
                startTime = clock();
                int operatorCode = checkRecord(zxyPtr, row, -1);
                finishTime = clock();
                if (operatorCode == 1) {
                    cout << "check successful, time: " << durationTime(&finishTime, &startTime) << " seconds\n" << nextLineHeader;
                } else {
                    cout << "> failed!\n" << nextLineHeader;
                }
            } else {
                cout << errorMessage << nextLineHeader;
            }

        } else if (strncmp(userCommand, "index", 5) == 0) {
            ll col;
            int inputValid = sscanf(userCommand, "index %lld;", &col);
            if (inputValid == 1) {
                startTime = clock();
                int operatorCode = initIndex(zxyPtr, col);
                finishTime = clock();
                if (operatorCode == 1) {
                    cout << "init " << col << ".bin successful, time: " << durationTime(&finishTime, &startTime) << " seconds\n" << nextLineHeader;
                } else {
                    cout << "> failed!\n" << nextLineHeader;
                }
            } else {
                cout << errorMessage << nextLineHeader;
            }
        }
    }
}

int insertRandomRecord(Table *tablePtr, int num) {
    srand((unsigned)time(NULL));
    int flag = 0;
    for (int i = 0; i < num; i++) {
        vector<ll> data;
        ll s;
        for (int j = 0; j < tablePtr->colLen; j++) {
            s = rand() % 1000;
            data.push_back(s);
        }
        if (zxyPtr->append(data) == 0) {
            flag++;
        }
    }
    if (flag == num) {
        return 1;
    } else if (flag == 0) {
        return -1;
    } else {
        return 0;
    }
}

void printInfo(Table *tablePtr) {
    cout << "table's name: " << tablePtr->name << endl
         << "there is " << tablePtr->count << " record" << endl << nextLineHeader;
}

int searchRecord(Table* tablePtr, ll col, ll min, ll max, vector<ll> &searchResult) {
    return tablePtr-> search(col, min, max, searchResult);
}

int checkRecord(Table* tablePtr, ll row, ll col) {
    vector<ll> data;
    data = tablePtr->dataCp[row];
    if (col == -1) {
        cout << "row " << row << ": " << endl;
        for (int i = 0; i < data.size(); i++) {
            cout << data[i] << "-";
        }
        cout << endl << nextLineHeader;
    } else {
        cout << "row " << row << ", column " << col << ": " << endl;
        cout << data[col] << endl << nextLineHeader;
    }
    return 1;
}

int initIndex(Table* tablePtr, ll col) {
    
    tablePtr->initIndex(col);

    return 1;
}

double durationTime(clock_t *f,clock_t *s){
	return (double)(*f - *s) / CLOCKS_PER_SEC;	
}

//test
void *testTable(void *threadid);

void *testTable(void *threadid) {
    int tid = *((int*)threadid);
    Table* tablePtr = Table::getInstance();
    cout << "there is thread-" << tid << endl;
    cout << tid << "-table's mutex is " << tablePtr->mutex << endl;
    vector<ll> testData;
    for (ll i = 0; i < tablePtr->colLen; i++) {
        ll s = rand() % 1000;
        testData.push_back(s);
    }
    tablePtr->append(testData);
    cout << tid << "-table's length is " << tablePtr->count << endl;
    pthread_exit(NULL);
}



int main() {
    // srand((unsigned)time(NULL));
    // pthread_t threads[NUM_THREADS];
    // int indics[NUM_THREADS];

    // int rc;

    // for (int i = 0; i < NUM_THREADS; i++) {
    //     cout << "main() : 创建线程, " << i << endl;
    //     indics[i] = i;
    //     rc = pthread_create(&threads[i], NULL, testTable, (void *)&(indics[i]));

    // }
    // pthread_exit(NULL);

    initialSystem();

    return 0;
}

