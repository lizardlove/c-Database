#include <iostream>
#include <stdlib.h>
#include <list>
#include <string.h>
#include <string>
#include <algorithm>
#include "BPlusTree.h"

using std::string;
using std::swap;
using std::binary_search;
using std::lower_bound;
using std::upper_bound;

namespace bpt {

    #define MAXRANGE 10

    OPERATOR_KEYCMP(index_t)
    OPERATOR_KEYCMP(record_t)

    /* 迭代器 */
    template<class T>
    inline typename T::child_t begin(T &node) {
        return node.children;
    }
    template<class T>
    inline typename T::child_t end(T &node) {
        return node.children + node.n;
    }

    /*比较器*/
    inline index_t *find(internal_node_t &node, const ll &key) {
        return upper_bound(begin(node), end(node) - 1, key);
    }
    inline record_t *find(leaf_node_t &node, const ll &key) {
        return lower_bound(begin(node), end(node), key);
    }

    BPlusTree::BPlusTree(const char *p, bool force_empty) : fp(0), fp_level(0)
    {
        bzero(path, sizeof(path));
        strcpy(path, p);

        if (!force_empty) {
            //文件存在
            if(map(&meta, OFFSET_META) != 0) {
                force_empty = true;
            }
        } else if (force_empty) {
            //文件不存在
            open_file();
            init_from_empty();
            close_file();
        }
    }

    int BPlusTree::search(const ll key, ll *values, size_t max) const
    {
        leaf_node_t leaf;
        map(&leaf, search_leaf(key));
        // 搜索记录节点
        int j, i = 0;
        record_t *record;
        for (j = 0; j < leaf.n && i < max; j++) {
            if (leaf.children[j].key == key) {
                *(values+i) = leaf.children[j].value;
                i++;
            }
        }
        record = leaf.children + leaf.n;
        while(record->value == key && i < max) {
            if (record == leaf.children + leaf.n && leaf.next) {
                map(&leaf, leaf.next);
                j=0;
            }
            record = leaf.children + j;
            if (leaf.children[j].key == key) {
                *(values+i) = leaf.children[j].value;
                i++;
            }
            j++;
        }

        if (i == 0) {
            return -1; 
        } else {
            return 1;
        }
    }

    int BPlusTree::search_range(ll &left, const ll right,
                             ll *values, size_t max, bool *next) const
    {
        if (left == 0 || left > right)
            return -1;

        off_t off_left = search_leaf(left);
        off_t off_right = search_leaf(right);
        off_t off = off_left;
        size_t i = 0;
        record_t *b, *e;

        leaf_node_t leaf;
        while (off != off_right && off != 0 && i < max) {
            map(&leaf, off);

            // 起始叶块
            if (off_left == off) 
                b = find(leaf, left);
            else
                b = begin(leaf);

            // 复制到数组
            e = leaf.children + leaf.n;
            for (; b != e && i < max; ++b, ++i)
                values[i] = b->value;

            off = leaf.next;
        }

        // 最后的叶块
        if (i < max) {
            map(&leaf, off_right);

            b = find(leaf, left);
            e = upper_bound(begin(leaf), end(leaf), right);
            for (; b != e && i < max; ++b, ++i)
                values[i] = b->value;
        }

        if (next != NULL) {
            if (i == max && b != e) {
                *next = true;
                left = b->key;
            } else {
                *next = false;
            }
        }

        return i;
    }

    int BPlusTree::remove(const ll key) {
        internal_node_t parent;
        leaf_node_t leaf;

        // 找索引块
        off_t parent_off = search_index(key);
        map(&parent, parent_off);

        // 对应叶块
        index_t *where = find(parent, key);
        off_t offset = where->child;
        map(&leaf, offset);

        // 验证
        if (!binary_search(begin(leaf), end(leaf), key))
            return -1;

        size_t min_n = meta.leaf_node_num == 1 ? 0 : meta.order / 2;
        assert(leaf.n >= min_n && leaf.n <= meta.order);

        // 删除记录
        record_t *to_delete = find(leaf, key);
        std::copy(to_delete + 1, end(leaf), to_delete);
        leaf.n--;

        //拆分节点
        if (leaf.n < min_n) {
            // 往左借
            bool borrowed = false;
            if (leaf.prev != 0)
                borrowed = borrow_key(false, leaf);

            // 往右借
            if (!borrowed && leaf.next != 0)
                borrowed = borrow_key(true, leaf);

            // 优先向右合并
            if (!borrowed) {
                assert(leaf.next != 0 || leaf.prev != 0);

                ll index_key;

                if (where == end(parent) - 1) {
                    // 该叶块是父节点的最后一个子元素
                    assert(leaf.prev != 0);
                    leaf_node_t prev;
                    map(&prev, leaf.prev);
                    index_key = begin(prev)->key;

                    merge_leafs(&prev, &leaf);
                    node_remove(&prev, &leaf);
                    unmap(&prev, leaf.prev);
                } else {
                    // 向右合并
                    assert(leaf.next != 0);
                    leaf_node_t next;
                    map(&next, leaf.next);
                    index_key = begin(leaf)->key;

                    merge_leafs(&leaf, &next);
                    node_remove(&leaf, &next);
                    unmap(&leaf, offset);
                }

                // 移除键值
                remove_from_index(parent_off, parent, index_key);
            } else {
                unmap(&leaf, offset);
            }
        } else {
            unmap(&leaf, offset);
        }

        return 0;
    }

    int BPlusTree::insert(const ll key, ll value) {
        off_t parent = search_index(key);
        off_t offset = search_leaf(parent, key);
        leaf_node_t leaf;
        map(&leaf, offset);

        // 检测是否存在
        // if (binary_search(begin(leaf), end(leaf), key))
        //     return 1;

        if (leaf.n == meta.order) {
            // 满阶

            // 分一个新的叶节点
            leaf_node_t new_leaf;
            node_create(offset, &leaf, &new_leaf);

            // 确定分离位置
            size_t point = leaf.n / 2;
            bool place_right = key > leaf.children[point].key;
            if (place_right)
                ++point;

            // 分离节点数据
            std::copy(leaf.children + point, leaf.children + leaf.n,
                    new_leaf.children);
            new_leaf.n = leaf.n - point;
            leaf.n = point;

            if (place_right)
                insert_record_no_split(&new_leaf, key, value);
            else
                insert_record_no_split(&leaf, key, value);

            // 保存数据
            unmap(&leaf, offset);
            unmap(&new_leaf, leaf.next);

            // 插入新记录
            insert_key_to_index(parent, new_leaf.children[0].key,
                                offset, leaf.next);
        } else {
            insert_record_no_split(&leaf, key, value);
            unmap(&leaf, offset);
        }
        unmap(&meta, OFFSET_META);
        return 0;
    }

    int BPlusTree::update(const ll key, ll value) {
        off_t offset = search_leaf(key);
        leaf_node_t leaf;
        map(&leaf, offset);

        record_t *record = find(leaf, key);
        if (record != leaf.children + leaf.n)
            if (key == record->key) {
                record->value = value;
                unmap(&leaf, offset);

                return 0;
            } else {
                return 1;
            }
        else
            return -1;
    }

    void BPlusTree::remove_from_index(off_t offset, internal_node_t &node, const ll &key) {
        size_t min_n = meta.root_offset == offset ? 1 : meta.order / 2;
        assert(node.n >= min_n && node.n <= meta.order);

        // 移除键
        ll index_key = begin(node)->key;
        index_t *to_delete = find(node, key);
        if (to_delete != end(node)) {
            (to_delete + 1)->child = to_delete->child;
            std::copy(to_delete + 1, end(node), to_delete);
        }
        node.n--;

        // 释放根节点
        if (node.n == 1 && meta.root_offset == offset &&
                        meta.internal_node_num != 1)
        {
            unalloc(&node, meta.root_offset);
            meta.height--;
            meta.root_offset = node.children[0].child;
            unmap(&meta, OFFSET_META);
            return;
        }

        // 小于限制则合并
        if (node.n < min_n) {
            internal_node_t parent;
            map(&parent, node.parent);

            // 向左借
            bool borrowed = false;
            if (offset != begin(parent)->child)
                borrowed = borrow_key(false, node, offset);

            // 向右借
            if (!borrowed && offset != (end(parent) - 1)->child)
                borrowed = borrow_key(true, node, offset);

            // 优先向右合并
            if (!borrowed) {
                assert(node.next != 0 || node.prev != 0);

                if (offset == (end(parent) - 1)->child) {
                    // 最后的节点
                    assert(node.prev != 0);
                    internal_node_t prev;
                    map(&prev, node.prev);

                    index_t *where = find(parent, begin(prev)->key);
                    reset_index_children_parent(begin(node), end(node), node.prev);
                    merge_keys(where, prev, node);
                    unmap(&prev, node.prev);
                } else {
                    assert(node.next != 0);
                    internal_node_t next;
                    map(&next, node.next);

                    index_t *where = find(parent, index_key);
                    reset_index_children_parent(begin(next), end(next), offset);
                    merge_keys(where, node, next);
                    unmap(&node, offset);
                }

                //释放
                remove_from_index(node.parent, parent, index_key);
            } else {
                unmap(&node, offset);
            }
        } else {
            unmap(&node, offset);
        }
    }

    bool BPlusTree::borrow_key(bool from_right, internal_node_t &borrower, off_t offset) {
        typedef typename internal_node_t::child_t child_t;

        off_t lender_off = from_right ? borrower.next : borrower.prev;
        internal_node_t lender;
        map(&lender, lender_off);

        assert(lender.n >= meta.order / 2);
        if (lender.n != meta.order / 2) {
            child_t where_to_lend, where_to_put;

            internal_node_t parent;

            // 迁移子节点数据
            if (from_right) {
                where_to_lend = begin(lender);
                where_to_put = end(borrower);

                map(&parent, borrower.parent);
                child_t where = lower_bound(begin(parent), end(parent) - 1, (end(borrower) -1)->key);
                where->key = where_to_lend->key;
                unmap(&parent, borrower.parent);
            } else {
                where_to_lend = end(lender) - 1;
                where_to_put = begin(borrower);

                map(&parent, lender.parent);
                child_t where = find(parent, begin(lender)->key);
                where_to_put->key = where->key;
                where->key = (where_to_lend - 1)->key;
                unmap(&parent, lender.parent);
            }

            // 保存
            std::copy_backward(where_to_put, end(borrower), end(borrower) + 1);
            *where_to_put = *where_to_lend;
            borrower.n++;

            // 释放数据
            reset_index_children_parent(where_to_lend, where_to_lend + 1, offset);
            std::copy(where_to_lend + 1, end(lender), where_to_lend);
            lender.n--;
            unmap(&lender, lender_off);
            return true;
        }

        return false;
    }
    
    bool BPlusTree::borrow_key(bool from_right, leaf_node_t &borrower) {
        off_t lender_off = from_right ? borrower.next : borrower.prev;
        leaf_node_t lender;
        map(&lender, lender_off);

        assert(lender.n >= meta.order / 2);
        if (lender.n != meta.order / 2) {
            typename leaf_node_t::child_t where_to_lend, where_to_put;

            if (from_right) {
                where_to_lend = begin(lender);
                where_to_put = end(borrower);
                change_parent_child(borrower.parent, begin(borrower)->key,
                                    lender.children[1].key);
            } else {
                where_to_lend = end(lender) - 1;
                where_to_put = begin(borrower);
                change_parent_child(lender.parent, begin(lender)->key,
                                    where_to_lend->key);
            }

            std::copy_backward(where_to_put, end(borrower), end(borrower) + 1);
            *where_to_put = *where_to_lend;
            borrower.n++;

            // 释放数据
            std::copy(where_to_lend + 1, end(lender), where_to_lend);
            lender.n--;
            unmap(&lender, lender_off);
            return true;
        }

        return false;
    }

    void BPlusTree::change_parent_child(off_t parent, const ll &o, const ll &n) {
        internal_node_t node;
        map(&node, parent);

        index_t *w = find(node, o);
        assert(w != node.children + node.n); 

        w->key = n;
        unmap(&node, parent);
        //向上迭代
        if (w == node.children + node.n - 1) {
            change_parent_child(node.parent, o, n);
        }
    }

    void BPlusTree::merge_leafs(leaf_node_t *left, leaf_node_t *right) {
        std::copy(begin(*right), end(*right), end(*left));
        left->n += right->n;
    }

    void BPlusTree::merge_keys(index_t *where, internal_node_t &node, internal_node_t &next) {
        std::copy(begin(next), end(next), end(node));
        node.n += next.n;
        node_remove(&node, &next);
    }

    void BPlusTree::insert_record_no_split(leaf_node_t *leaf, const ll &key, const ll &value) {
        record_t *where = upper_bound(begin(*leaf), end(*leaf), key);
        std::copy_backward(where, end(*leaf), end(*leaf) + 1);

        where->key = key;
        where->value = value;
        leaf->n++;
    }

    void BPlusTree::insert_key_to_index(off_t offset, const ll &key, off_t old, off_t after) {
        if (offset == 0) {
            // 无数据时，新建根节点
            internal_node_t root;
            root.next = root.prev = root.parent = 0;
            meta.root_offset = alloc(&root);
            meta.height++;

            // 插入前后数据
            root.n = 2;
            root.children[0].key = key;
            root.children[0].child = old;
            root.children[1].child = after;

            unmap(&meta, OFFSET_META);
            unmap(&root, meta.root_offset);

            // 更新子节点
            reset_index_children_parent(begin(root), end(root),
                                        meta.root_offset);
            return;
        }

        internal_node_t node;
        map(&node, offset);
        assert(node.n <= meta.order);

        if (node.n == meta.order) {
            // 满阶

            internal_node_t new_node;
            node_create(offset, &node, &new_node);

            // 分割点
            size_t point = (node.n - 1) / 2;
            bool place_right = (key > node.children[point].key);
            if (place_right)
                ++point;

            if (!place_right && key < node.children[point].key)
                point--;

            ll middle_key = node.children[point].key;

            
            std::copy(begin(node) + point + 1, end(node), begin(new_node));
            new_node.n = node.n - point - 1;
            node.n = point + 1;

            if (place_right)
                insert_key_to_index_no_split(new_node, key, after);
            else
                insert_key_to_index_no_split(node, key, after);

            unmap(&node, offset);
            unmap(&new_node, node.next);

            // 更新父节点
            reset_index_children_parent(begin(new_node), end(new_node), node.next);

            // 中值向上传递
            insert_key_to_index(node.parent, middle_key, offset, node.next);
        } else {
            insert_key_to_index_no_split(node, key, after);
            unmap(&node, offset);
        }
    } 

    void BPlusTree::insert_key_to_index_no_split(internal_node_t &node, const ll &key, off_t value) {
        index_t *where = upper_bound(begin(node), end(node) - 1, key);

        //向后移动索引
        std::copy_backward(where, end(node), end(node) + 1);

        // 插入索引
        where->key = key;
        where->child = (where + 1)->child;
        (where + 1)->child = value;

        node.n++;
    }

    void BPlusTree::reset_index_children_parent(index_t *begin, index_t *end, off_t parent) {
        // 索引节点都是内部节点
        internal_node_t node;
        while (begin != end) {
            map(&node, begin->child);
            node.parent = parent;
            unmap(&node, begin->child, SIZE_NO_CHILDREN);
            ++begin;
        }
    }

    off_t BPlusTree::search_index(const ll &key) const {
        off_t org = meta.root_offset;
        int height = meta.height;
        while (height > 1) {
            internal_node_t node;
            map(&node, org);

            index_t *i = upper_bound(begin(node), end(node) - 1, key);
            org = i->child;
            --height;
        }

        return org;
    }

    off_t BPlusTree::search_leaf(off_t index, const ll &key) const {
        internal_node_t node;
        map(&node, index);

        index_t *i = upper_bound(begin(node), end(node) - 1, key);
        return i->child;
    }

    template<class T>
    void BPlusTree::node_create(off_t offset, T *node, T *next) {
        next->parent = node->parent;
        next->next = node->next;
        next->prev = offset;
        node->next = alloc(next);

        if (next->next != 0) {
            T old_next;
            map(&old_next, next->next, SIZE_NO_CHILDREN);
            old_next.prev = node->next;
            unmap(&old_next, next->next, SIZE_NO_CHILDREN);
        }
        unmap(&meta, OFFSET_META);
    }

    template<class T>
    void BPlusTree::node_remove(T *prev, T *node) {
        unalloc(node, prev->next);
        prev->next = node->next;
        if (node->next != 0) {
            T next;
            map(&next, node->next, SIZE_NO_CHILDREN);
            next.prev = node->prev;
            unmap(&next, node->next, SIZE_NO_CHILDREN);
        }
        unmap(&meta, OFFSET_META);
    }

    void BPlusTree::init_from_empty() {
        // init default meta
        bzero(&meta, sizeof(meta_t));
        meta.order = BP_ORDER;
        meta.value_size = sizeof(ll);
        meta.key_size = sizeof(ll);
        meta.height = 1;
        meta.slot = OFFSET_BLOCK;

        // init root node
        internal_node_t root;
        root.next = root.prev = root.parent = 0;
        meta.root_offset = alloc(&root);

        // init empty leaf
        leaf_node_t leaf;
        leaf.next = leaf.prev = 0;
        leaf.parent = meta.root_offset;
        meta.leaf_offset = root.children[0].child = alloc(&leaf);

        // save
        unmap(&meta, OFFSET_META);
        unmap(&root, meta.root_offset);
        unmap(&leaf, root.children[0].child);
    }

}
