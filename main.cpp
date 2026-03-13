#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cstdio>

using namespace std;

const int MAX_KEY_SIZE = 64;
const int ORDER = 100; // B+ tree order

struct Entry {
    char index[MAX_KEY_SIZE + 1];
    int value;

    Entry() {
        memset(index, 0, sizeof(index));
        value = 0;
    }

    Entry(const char* idx, int val) {
        memset(index, 0, sizeof(index));
        strncpy(index, idx, MAX_KEY_SIZE);
        index[MAX_KEY_SIZE] = '\0';
        value = val;
    }

    bool operator<(const Entry& other) const {
        int cmp = strcmp(index, other.index);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }

    bool operator==(const Entry& other) const {
        return strcmp(index, other.index) == 0 && value == other.value;
    }
};

struct Node {
    bool is_leaf;
    int n;
    Entry keys[ORDER];
    int children[ORDER + 1];
    int next;

    Node() {
        is_leaf = true;
        n = 0;
        for (int i = 0; i <= ORDER; i++) {
            children[i] = -1;
        }
        next = -1;
    }
};

class BPlusTree {
private:
    FILE* file;
    string filename;
    int root_pos;
    int node_count;

    void write_node(int pos, const Node& node) {
        fseek(file, pos * sizeof(Node), SEEK_SET);
        fwrite(&node, sizeof(Node), 1, file);
        fflush(file);
    }

    Node read_node(int pos) {
        Node node;
        fseek(file, pos * sizeof(Node), SEEK_SET);
        fread(&node, sizeof(Node), 1, file);
        return node;
    }

    int allocate_node() {
        return node_count++;
    }

    void insert_in_leaf(Node& leaf, const Entry& entry) {
        int i = leaf.n - 1;
        while (i >= 0 && entry < leaf.keys[i]) {
            leaf.keys[i + 1] = leaf.keys[i];
            i--;
        }
        leaf.keys[i + 1] = entry;
        leaf.n++;
    }

    void split_child(int parent_pos, int child_idx) {
        Node parent = read_node(parent_pos);
        int child_pos = parent.children[child_idx];
        Node child = read_node(child_pos);

        Node new_node;
        new_node.is_leaf = child.is_leaf;

        int mid = (ORDER + 1) / 2;

        if (child.is_leaf) {
            // Split leaf node
            new_node.n = child.n - mid;
            for (int i = 0; i < new_node.n; i++) {
                new_node.keys[i] = child.keys[mid + i];
            }
            child.n = mid;

            new_node.next = child.next;
            int new_pos = allocate_node();
            child.next = new_pos;

            // Insert into parent
            for (int i = parent.n; i > child_idx + 1; i--) {
                parent.children[i + 1] = parent.children[i];
            }
            for (int i = parent.n - 1; i >= child_idx; i--) {
                parent.keys[i + 1] = parent.keys[i];
            }

            parent.keys[child_idx] = new_node.keys[0];
            parent.children[child_idx + 1] = new_pos;
            parent.n++;

            write_node(child_pos, child);
            write_node(new_pos, new_node);
            write_node(parent_pos, parent);
        } else {
            // Split internal node
            new_node.n = child.n - mid;
            for (int i = 0; i < new_node.n; i++) {
                new_node.keys[i] = child.keys[mid + i];
                new_node.children[i] = child.children[mid + i];
            }
            new_node.children[new_node.n] = child.children[child.n];

            Entry push_up_key = child.keys[mid - 1];
            child.n = mid - 1;

            int new_pos = allocate_node();

            // Insert into parent
            for (int i = parent.n; i > child_idx + 1; i--) {
                parent.children[i + 1] = parent.children[i];
            }
            for (int i = parent.n - 1; i >= child_idx; i--) {
                parent.keys[i + 1] = parent.keys[i];
            }

            parent.keys[child_idx] = push_up_key;
            parent.children[child_idx + 1] = new_pos;
            parent.n++;

            write_node(child_pos, child);
            write_node(new_pos, new_node);
            write_node(parent_pos, parent);
        }
    }

    void insert_non_full(int node_pos, const Entry& entry) {
        Node node = read_node(node_pos);

        if (node.is_leaf) {
            insert_in_leaf(node, entry);
            write_node(node_pos, node);
        } else {
            int i = 0;
            while (i < node.n && node.keys[i] < entry) {
                i++;
            }

            int child_pos = node.children[i];
            Node child = read_node(child_pos);

            if (child.n >= ORDER) {
                split_child(node_pos, i);
                node = read_node(node_pos);
                if (node.keys[i] < entry || node.keys[i] == entry) {
                    i++;
                }
            }

            insert_non_full(node.children[i], entry);
        }
    }

public:
    BPlusTree(const string& fname) : filename(fname) {
        file = fopen(filename.c_str(), "rb+");

        if (!file) {
            // Create new file
            file = fopen(filename.c_str(), "wb+");
            root_pos = 0;
            node_count = 1;
            Node root;
            write_node(root_pos, root);
        } else {
            // File exists
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
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
        if (file) {
            fclose(file);
        }
    }

    void insert(const char* index, int value) {
        Entry entry(index, value);
        Node root = read_node(root_pos);

        if (root.n >= ORDER) {
            // Create new root
            Node new_root;
            new_root.is_leaf = false;
            new_root.children[0] = root_pos;

            int new_root_pos = allocate_node();
            int old_root_pos = root_pos;
            root_pos = new_root_pos;

            write_node(old_root_pos, root);
            write_node(new_root_pos, new_root);

            split_child(new_root_pos, 0);
        }

        insert_non_full(root_pos, entry);
    }

    void remove(const char* index, int value) {
        Entry target(index, value);

        // Find leaf node
        int node_pos = root_pos;
        Node node = read_node(node_pos);

        while (!node.is_leaf) {
            int i = 0;
            while (i < node.n && node.keys[i] < target) {
                i++;
            }
            node_pos = node.children[i];
            node = read_node(node_pos);
        }

        // Search and delete from leaf
        while (node_pos != -1) {
            node = read_node(node_pos);
            for (int i = 0; i < node.n; i++) {
                if (node.keys[i] == target) {
                    for (int j = i; j < node.n - 1; j++) {
                        node.keys[j] = node.keys[j + 1];
                    }
                    node.n--;
                    write_node(node_pos, node);
                    return;
                } else if (strcmp(node.keys[i].index, target.index) > 0) {
                    return;
                }
            }
            node_pos = node.next;
        }
    }

    vector<int> find(const char* index) {
        vector<int> result;

        // Find leftmost leaf
        int node_pos = root_pos;
        Node node = read_node(node_pos);

        Entry search;
        strncpy(search.index, index, MAX_KEY_SIZE);
        search.index[MAX_KEY_SIZE] = '\0';
        search.value = -2147483648;

        while (!node.is_leaf) {
            int i = 0;
            while (i < node.n && node.keys[i] < search) {
                i++;
            }
            node_pos = node.children[i];
            node = read_node(node_pos);
        }

        // Traverse leaves
        while (node_pos != -1) {
            node = read_node(node_pos);
            for (int i = 0; i < node.n; i++) {
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
