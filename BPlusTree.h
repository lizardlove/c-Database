#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define ll long long

namespace bpt {
    #define BP_ORDER 50  /*阶数*/

    #define OFFSET_META 0
    #define OFFSET_BLOCK OFFSET_META + sizeof(meta_t)
    #define SIZE_NO_CHILDREN sizeof(leaf_node_t) - BP_ORDER * sizeof(record_t)

    #define OPERATOR_KEYCMP(type) \
        bool operator< (const ll &l, const type &r) {\
            return l < r.key;\
        }\
        bool operator< (const type &l, const ll &r) {\
            return l.key <  r;\
        }\
        bool operator== (const ll &l, const type &r) {\
            return l == r.key;\
        }\
        bool operator== (const type &l, const ll &r) {\
            return l.key == r;\
        }


    typedef struct {
        size_t order; /* B+树-阶 */

        size_t value_size; /* size of value */
        size_t key_size;   /* size of key */

        size_t internal_node_num; /* 内部节点个数 */
        size_t leaf_node_num;     /* 叶节点个数 */
        size_t height;            /* 树高度 */
        off_t slot;        /* 新块的位置（物理） */
        off_t root_offset; /* 根节点位置 */
        off_t leaf_offset; /* 第一个叶节点位置 */
    } meta_t;

    /* 索引值 */
    struct index_t {
        ll key;
        off_t child; 
    };

    /***
     * 内部节点块
     ***/
    struct internal_node_t {
        typedef index_t * child_t;
        
        off_t parent; 
        off_t next;
        off_t prev;
        size_t n; /* 节点内子块数 */
        index_t children[BP_ORDER];
    };

    /* 记录值 */
    struct record_t {
        ll key;
        off_t value;
    };

    /* 叶节点块 */
    struct leaf_node_t {
        typedef record_t *child_t;

        off_t parent; /* 父节点*/
        off_t next;
        off_t prev;
        size_t n;
        record_t children[BP_ORDER];
    };

    class BPlusTree {
        public:
            BPlusTree(const char *path, bool force_empty = false);

            /* 键-值 搜索 */
            int search(const ll key, ll *values, size_t max) const;
            /* 范围搜索 */
            int search_range(ll &left, const ll right, ll *values, size_t max, bool *next=NULL) const;

            int remove(const ll key);
            int insert(const ll key, ll value);
            int update(const ll key, ll value);
            meta_t get_meta() const {
                return meta;
            }


            char path[512];
            meta_t meta;

            void init_from_empty();

            /*按键搜索 索引节点*/
            off_t search_index(const ll &key) const;
            /*按键搜索 叶子节点*/
            off_t search_leaf(off_t index, const ll &key) const;
            off_t search_leaf(const ll &key) const {
                return search_leaf(search_index(key), key);
            }

            /* 移除索引节点 */
            void remove_from_index(off_t offset, internal_node_t &node, const ll &key);

            /* 迁移索引 */
            bool borrow_key(bool from_right, internal_node_t &borrower, off_t offset);
            /* 迁移记录 */
            bool borrow_key(bool from_right, leaf_node_t &borrower);

            /* 记录值父节点变动 */
            void change_parent_child(off_t parent, const ll &o, const ll &n);

            /*合并叶块*/
            void merge_leafs(leaf_node_t *left, leaf_node_t *right);
            /*合并索引块*/
            void merge_keys(index_t *where, internal_node_t &left, internal_node_t &right);

            /* 叶块插入记录 */
            void insert_record_no_split(leaf_node_t *leaf, const ll &key, const ll &value);

            /* 内部块插入索引 */
            void insert_key_to_index(off_t offset, const ll &key, off_t value, off_t after);
            void insert_key_to_index_no_split(internal_node_t &node, const ll &key, off_t value);

            /*内部块父节点变动*/
            void reset_index_children_parent(index_t *begin, index_t *end, off_t parent);

            template<class T>
            void node_create(off_t offset, T *node, T *next);

            template<class T>
            void node_remove(T *prev, T *node);

            mutable int fp;   /*文件描述符*/
            mutable int fp_level;  /*访问控制*/
            void open_file() const {
                if (fp_level == 0) {
                    fp = open(path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IXUSR);
                }
                ++fp_level;
            }

            void close_file() const {
                if (fp_level == 1) {
                    close(fp);
                }
                --fp_level;
            }

            /*申请空间*/
            off_t alloc(size_t size) {
                off_t slot = meta.slot;
                meta.slot += size;
                return slot;
            }
            off_t alloc(leaf_node_t *leaf) {
                leaf->n = 0;
                meta.leaf_node_num++;

                return alloc(sizeof(leaf_node_t));
            }
            off_t alloc(internal_node_t *node) {
                node->n = 1;
                meta.internal_node_num++;
                return alloc(sizeof(internal_node_t));
            }

            /*释放空间*/
            void unalloc(internal_node_t *node, off_t offset) {
                --meta.internal_node_num;
            }
            void unalloc(leaf_node_t *leaf, off_t offset) {
                --meta.leaf_node_num;
            }

            /*读数据*/
            int map(void *block, off_t offset, size_t size) const {
                open_file();
                lseek(fp, offset, SEEK_SET);
                size_t rd = read(fp, block, size);
                close_file();

                return rd;
            }
            template<class T>
            int map(T *block, off_t offset) const {
                return map(block, offset, sizeof(T));
            }

            /*写数据*/
            int unmap(void *block, off_t offset, size_t size) const {
                open_file();
                lseek(fp, offset, SEEK_SET);
                size_t wd = write(fp, block, size);
                close_file();

                return wd;
            }
            template<class T>
            int unmap(T *block, off_t offset) const {
                return unmap(block, offset, sizeof(T));
            }
    };

}


#endif