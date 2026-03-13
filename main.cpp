#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const int BLOCK_SIZE = 4096;
const int MAX_KEY_SIZE = 64;
const int M = 100; // B+ tree order

struct Key {
    char index[MAX_KEY_SIZE + 1];
    int value;

    Key() {
        memset(index, 0, sizeof(index));
        value = 0;
    }

    Key(const char* idx, int val) {
        memset(index, 0, sizeof(index));
        strncpy(index, idx, MAX_KEY_SIZE);
        value = val;
    }

    bool operator<(const Key& other) const {
        int cmp = strcmp(index, other.index);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }

    bool operator==(const Key& other) const {
        return strcmp(index, other.index) == 0 && value == other.value;
    }

    bool operator<=(const Key& other) const {
        return *this < other || *this == other;
    }
};

struct Node {
    bool is_leaf;
    int key_count;
    Key keys[M];
    int children[M + 1]; // file positions for internal nodes, or -1
    int next; // for leaf nodes, pointer to next leaf

    Node() {
        is_leaf = true;
        key_count = 0;
        memset(children, -1, sizeof(children));
        next = -1;
    }
};

class BPlusTree {
private:
    fstream file;
    string filename;
    int root_pos;
    int node_count;

    int allocate_node() {
        return node_count++;
    }

    void write_node(int pos, const Node& node) {
        file.seekp(pos * sizeof(Node));
        file.write((char*)&node, sizeof(Node));
        file.flush();
    }

    Node read_node(int pos) {
        Node node;
        file.seekg(pos * sizeof(Node));
        file.read((char*)&node, sizeof(Node));
        return node;
    }

    void split_child(Node& parent, int index, Node& child) {
        Node new_child;
        new_child.is_leaf = child.is_leaf;

        int mid = (M + 1) / 2;
        new_child.key_count = child.key_count - mid;

        for (int i = 0; i < new_child.key_count; i++) {
            new_child.keys[i] = child.keys[mid + i];
        }

        if (!child.is_leaf) {
            for (int i = 0; i <= new_child.key_count; i++) {
                new_child.children[i] = child.children[mid + i];
            }
        }

        child.key_count = mid;

        if (child.is_leaf) {
            new_child.next = child.next;
            int new_pos = allocate_node();
            child.next = new_pos;
            write_node(new_pos, new_child);

            for (int i = parent.key_count; i > index + 1; i--) {
                parent.children[i + 1] = parent.children[i];
            }
            for (int i = parent.key_count - 1; i >= index; i--) {
                parent.keys[i + 1] = parent.keys[i];
            }

            parent.keys[index] = new_child.keys[0];
            parent.children[index + 1] = new_pos;
            parent.key_count++;
        } else {
            int new_pos = allocate_node();
            write_node(new_pos, new_child);

            for (int i = parent.key_count; i > index + 1; i--) {
                parent.children[i + 1] = parent.children[i];
            }
            for (int i = parent.key_count - 1; i >= index; i--) {
                parent.keys[i + 1] = parent.keys[i];
            }

            parent.keys[index] = child.keys[mid - 1];
            parent.children[index + 1] = new_pos;
            parent.key_count++;

            child.key_count--;
        }
    }

    void insert_non_full(int node_pos, const Key& key) {
        Node node = read_node(node_pos);

        if (node.is_leaf) {
            int i = node.key_count - 1;
            while (i >= 0 && key < node.keys[i]) {
                node.keys[i + 1] = node.keys[i];
                i--;
            }
            node.keys[i + 1] = key;
            node.key_count++;
            write_node(node_pos, node);
        } else {
            int i = node.key_count - 1;
            while (i >= 0 && key < node.keys[i]) {
                i--;
            }
            i++;

            Node child = read_node(node.children[i]);
            if (child.key_count == M) {
                split_child(node, i, child);
                write_node(node.children[i], child);
                write_node(node_pos, node);

                if (node.keys[i] < key || node.keys[i] == key) {
                    i++;
                }
            }
            insert_non_full(node.children[i], key);
        }
    }

    void remove_from_leaf(Node& node, int index) {
        for (int i = index; i < node.key_count - 1; i++) {
            node.keys[i] = node.keys[i + 1];
        }
        node.key_count--;
    }

    bool delete_key(int node_pos, const Key& key) {
        Node node = read_node(node_pos);

        if (node.is_leaf) {
            for (int i = 0; i < node.key_count; i++) {
                if (node.keys[i] == key) {
                    remove_from_leaf(node, i);
                    write_node(node_pos, node);
                    return true;
                }
            }
            return false;
        } else {
            int i = 0;
            while (i < node.key_count && node.keys[i] < key) {
                i++;
            }

            if (node.children[i] != -1) {
                return delete_key(node.children[i], key);
            }
            return false;
        }
    }

public:
    BPlusTree(const string& fname) : filename(fname) {
        file.open(filename, ios::in | ios::out | ios::binary);

        if (!file.is_open()) {
            // File doesn't exist, create new
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);

            root_pos = 0;
            node_count = 1;
            Node root;
            write_node(root_pos, root);
        } else {
            // File exists, read metadata
            file.seekg(0, ios::end);
            long size = file.tellg();
            if (size == 0) {
                root_pos = 0;
                node_count = 1;
                Node root;
                write_node(root_pos, root);
            } else {
                root_pos = 0;
                node_count = size / sizeof(Node);
            }
        }
    }

    ~BPlusTree() {
        if (file.is_open()) {
            file.close();
        }
    }

    void insert(const char* index, int value) {
        Key key(index, value);
        Node root = read_node(root_pos);

        if (root.key_count == M) {
            Node new_root;
            new_root.is_leaf = false;
            new_root.key_count = 0;
            new_root.children[0] = root_pos;

            int new_root_pos = allocate_node();
            root_pos = new_root_pos;

            split_child(new_root, 0, root);
            write_node(0, root);
            write_node(new_root_pos, new_root);

            insert_non_full(new_root_pos, key);
        } else {
            insert_non_full(root_pos, key);
        }
    }

    void remove(const char* index, int value) {
        Key key(index, value);
        delete_key(root_pos, key);
    }

    vector<int> find(const char* index) {
        vector<int> result;

        // Find the leftmost leaf
        int node_pos = root_pos;
        Node node = read_node(node_pos);

        while (!node.is_leaf) {
            int i = 0;
            while (i < node.key_count && strcmp(node.keys[i].index, index) <= 0) {
                i++;
            }
            if (i > 0 && strcmp(node.keys[i - 1].index, index) == 0) {
                i--;
            }
            node_pos = node.children[i];
            node = read_node(node_pos);
        }

        // Traverse leaf nodes
        while (node_pos != -1) {
            node = read_node(node_pos);
            for (int i = 0; i < node.key_count; i++) {
                if (strcmp(node.keys[i].index, index) == 0) {
                    result.push_back(node.keys[i].value);
                } else if (strcmp(node.keys[i].index, index) > 0) {
                    sort(result.begin(), result.end());
                    return result;
                }
            }
            node_pos = node.next;
        }

        sort(result.begin(), result.end());
        return result;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);

    BPlusTree tree("data_file");

    int n;
    cin >> n;

    string cmd;
    for (int i = 0; i < n; i++) {
        cin >> cmd;

        if (cmd == "insert") {
            char index[MAX_KEY_SIZE + 1];
            int value;
            cin >> index >> value;
            tree.insert(index, value);
        } else if (cmd == "delete") {
            char index[MAX_KEY_SIZE + 1];
            int value;
            cin >> index >> value;
            tree.remove(index, value);
        } else if (cmd == "find") {
            char index[MAX_KEY_SIZE + 1];
            cin >> index;
            vector<int> result = tree.find(index);

            if (result.empty()) {
                cout << "null\n";
            } else {
                for (size_t j = 0; j < result.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << result[j];
                }
                cout << "\n";
            }
        }
    }

    return 0;
}
