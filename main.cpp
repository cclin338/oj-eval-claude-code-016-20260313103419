#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const int MAX_KEY_SIZE = 64;
const int ORDER = 85; // B+ tree order - optimized for block size

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

    bool keyEquals(const char* idx) const {
        return strcmp(index, idx) == 0;
    }
};

struct Node {
    bool is_leaf;
    int n; // number of keys
    Entry keys[ORDER];
    int children[ORDER + 1]; // positions in file
    int next; // for leaf nodes

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

    int find_child_index(const Node& node, const Entry& entry) {
        int i = 0;
        while (i < node.n && entry.index[0] != '\0' && node.keys[i] < entry) {
            i++;
        }
        return i;
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

    void insert_in_parent(int left_pos, const Entry& key, int right_pos, vector<int>& path) {
        if (path.empty()) {
            // Create new root
            Node new_root;
            new_root.is_leaf = false;
            new_root.n = 1;
            new_root.keys[0] = key;
            new_root.children[0] = left_pos;
            new_root.children[1] = right_pos;

            int new_root_pos = allocate_node();
            root_pos = new_root_pos;
            write_node(new_root_pos, new_root);
            return;
        }

        int parent_pos = path.back();
        path.pop_back();
        Node parent = read_node(parent_pos);

        if (parent.n < ORDER) {
            // Insert in parent
            int i = parent.n - 1;
            while (i >= 0 && key < parent.keys[i]) {
                parent.keys[i + 1] = parent.keys[i];
                parent.children[i + 2] = parent.children[i + 1];
                i--;
            }
            parent.keys[i + 1] = key;
            parent.children[i + 2] = right_pos;
            parent.n++;
            write_node(parent_pos, parent);
        } else {
            // Split parent
            Node new_parent;
            new_parent.is_leaf = false;

            int mid = ORDER / 2;
            Entry entries[ORDER + 1];
            int child_ptrs[ORDER + 2];

            // Copy existing entries
            for (int i = 0; i < parent.n; i++) {
                entries[i] = parent.keys[i];
                child_ptrs[i] = parent.children[i];
            }
            child_ptrs[parent.n] = parent.children[parent.n];

            // Insert new entry
            int i = parent.n;
            while (i > 0 && key < entries[i - 1]) {
                entries[i] = entries[i - 1];
                child_ptrs[i + 1] = child_ptrs[i];
                i--;
            }
            entries[i] = key;
            child_ptrs[i + 1] = right_pos;

            // Split
            parent.n = mid;
            for (int i = 0; i < mid; i++) {
                parent.keys[i] = entries[i];
                parent.children[i] = child_ptrs[i];
            }
            parent.children[mid] = child_ptrs[mid];

            Entry split_key = entries[mid];

            new_parent.n = ORDER - mid;
            for (int i = 0; i < new_parent.n; i++) {
                new_parent.keys[i] = entries[mid + 1 + i];
                new_parent.children[i] = child_ptrs[mid + 1 + i];
            }
            new_parent.children[new_parent.n] = child_ptrs[ORDER + 1];

            int new_parent_pos = allocate_node();
            write_node(parent_pos, parent);
            write_node(new_parent_pos, new_parent);

            insert_in_parent(parent_pos, split_key, new_parent_pos, path);
        }
    }

    void insert_helper(int node_pos, const Entry& entry, vector<int>& path) {
        Node node = read_node(node_pos);

        if (node.is_leaf) {
            if (node.n < ORDER) {
                insert_in_leaf(node, entry);
                write_node(node_pos, node);
            } else {
                // Split leaf
                Node new_leaf;
                new_leaf.is_leaf = true;
                new_leaf.next = node.next;

                Entry entries[ORDER + 1];
                for (int i = 0; i < node.n; i++) {
                    entries[i] = node.keys[i];
                }

                // Insert new entry
                int i = node.n;
                while (i > 0 && entry < entries[i - 1]) {
                    entries[i] = entries[i - 1];
                    i--;
                }
                entries[i] = entry;

                // Split
                int mid = (ORDER + 1) / 2;
                node.n = mid;
                for (int i = 0; i < mid; i++) {
                    node.keys[i] = entries[i];
                }

                new_leaf.n = ORDER + 1 - mid;
                for (int i = 0; i < new_leaf.n; i++) {
                    new_leaf.keys[i] = entries[mid + i];
                }

                int new_leaf_pos = allocate_node();
                node.next = new_leaf_pos;
                write_node(node_pos, node);
                write_node(new_leaf_pos, new_leaf);

                insert_in_parent(node_pos, new_leaf.keys[0], new_leaf_pos, path);
            }
        } else {
            int idx = find_child_index(node, entry);
            path.push_back(node_pos);
            insert_helper(node.children[idx], entry, path);
        }
    }

public:
    BPlusTree(const string& fname) : filename(fname) {
        file.open(filename, ios::in | ios::out | ios::binary);

        if (!file.is_open() || file.peek() == EOF) {
            file.close();
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);

            root_pos = 0;
            node_count = 1;
            Node root;
            write_node(root_pos, root);
        } else {
            file.seekg(0, ios::end);
            long size = file.tellg();
            root_pos = 0;
            node_count = size / sizeof(Node);
        }
    }

    ~BPlusTree() {
        if (file.is_open()) {
            file.close();
        }
    }

    void insert(const char* index, int value) {
        Entry entry(index, value);
        vector<int> path;
        insert_helper(root_pos, entry, path);
    }

    void remove(const char* index, int value) {
        Entry target(index, value);

        // Find the leftmost leaf that might contain the target
        int node_pos = root_pos;
        Node node = read_node(node_pos);

        while (!node.is_leaf) {
            int i = 0;
            while (i < node.n && target.index[0] != '\0' && node.keys[i] < target) {
                i++;
            }
            node_pos = node.children[i];
            node = read_node(node_pos);
        }

        // Search through leaf nodes
        while (node_pos != -1) {
            node = read_node(node_pos);
            for (int i = 0; i < node.n; i++) {
                if (node.keys[i] == target) {
                    // Found the entry, remove it
                    for (int j = i; j < node.n - 1; j++) {
                        node.keys[j] = node.keys[j + 1];
                    }
                    node.n--;
                    write_node(node_pos, node);
                    return;
                } else if (strcmp(node.keys[i].index, target.index) > 0) {
                    // Past the key, not found
                    return;
                }
            }
            node_pos = node.next;
        }
    }

    vector<int> find(const char* index) {
        vector<int> result;

        // Find the leftmost leaf
        int node_pos = root_pos;
        Node node = read_node(node_pos);

        Entry search_entry;
        strncpy(search_entry.index, index, MAX_KEY_SIZE);
        search_entry.index[MAX_KEY_SIZE] = '\0';
        search_entry.value = -2147483648; // minimum int

        while (!node.is_leaf) {
            int i = 0;
            while (i < node.n && node.keys[i] < search_entry) {
                i++;
            }
            node_pos = node.children[i];
            node = read_node(node_pos);
        }

        // Traverse leaf nodes
        while (node_pos != -1) {
            node = read_node(node_pos);
            for (int i = 0; i < node.n; i++) {
                if (node.keys[i].keyEquals(index)) {
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
