#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <limits>
#include <map>
#include <algorithm>
using namespace std;

const int NUM_ROUTERS = 4;
const vector<string> ROUTER_NAMES = {"R0", "R1", "R2", "R3"};
const int INF = numeric_limits<int>::max();

const vector<vector<int>> TOPOLOGY = {
    {0, 10, 20, INF},
    {10, 0, INF, 20},
    {20, INF, 0, 10},
    {INF, 20, 10, 0}
};

struct RIBEntry {
    int next_hop;
    int cost;
    RIBEntry() : next_hop(-1), cost(INF) {}
    RIBEntry(int nh, int c) : next_hop(nh), cost(c) {}
};

map<int, vector<RIBEntry>> GLOBAL_RIB;

vector<RIBEntry> dijkstra(int source_router_idx) {
    vector<int> dist(NUM_ROUTERS, INF);
    vector<int> parent(NUM_ROUTERS, -1);
    vector<bool> visited(NUM_ROUTERS, false);
    dist[source_router_idx] = 0;
    for (int count = 0; count < NUM_ROUTERS - 1; ++count) {
        int u = -1;
        int min_dist = INF;
        for (int v = 0; v < NUM_ROUTERS; ++v) {
            if (!visited[v] && dist[v] < min_dist) {
                min_dist = dist[v];
                u = v;
            }
        }
        if (u == -1) break;
        visited[u] = true;
        for (int v = 0; v < NUM_ROUTERS; ++v) {
            int weight = TOPOLOGY[u][v];
            if (!visited[v] && weight != INF && dist[u] != INF && (long long)dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                parent[v] = u;
            }
        }
    }
    vector<RIBEntry> rib(NUM_ROUTERS);
    for (int dest = 0; dest < NUM_ROUTERS; ++dest) {
        if (dest == source_router_idx) {
            rib[dest] = RIBEntry(source_router_idx, 0);
        } else if (dist[dest] != INF) {
            int next_hop = dest;
            while (parent[next_hop] != source_router_idx && parent[next_hop] != -1) {
                next_hop = parent[next_hop];
            }
            rib[dest] = RIBEntry(next_hop, dist[dest]);
        }
    }
    return rib;
}

void print_routing_table(int router_idx, const vector<RIBEntry>& rib_entries) {
    cout << "Routing Table for Router " << ROUTER_NAMES[router_idx] << ":" << endl;
    cout << " Destination | Next Hop | Total Cost" << endl;
    cout << "-------------|----------|-----------" << endl;
    for (int dest = 0; dest < NUM_ROUTERS; ++dest) {
        if (dest == router_idx) continue;
        const RIBEntry& entry = rib_entries[dest];
        string next_hop_name = (entry.next_hop != -1) ? ROUTER_NAMES[entry.next_hop] : "N/A";
        string cost_str = (entry.cost != INF) ? to_string(entry.cost) : "INF";
        cout << " " << ROUTER_NAMES[dest] << setw(10) << " | "
             << next_hop_name << setw(7) << " | "
             << cost_str << endl;
    }
    cout << endl;
}

enum LabelOperation { PUSH, SWAP, POP };

struct LFIBEntry {
    LabelOperation operation;
    int out_label;
    int next_hop;
    string fec;
    LFIBEntry() : operation(PUSH), out_label(-1), next_hop(-1), fec("") {}
    LFIBEntry(LabelOperation op, int out_l, int nh, string f) : operation(op), out_label(out_l), next_hop(nh), fec(f) {}
    LFIBEntry(LabelOperation op, int out_l, int nh) : operation(op), out_label(out_l), next_hop(nh), fec("") {}
};

map<int, map<int, LFIBEntry>> GLOBAL_LFIB;
map<int, map<string, LFIBEntry>> INGRESS_LFIB;

void setup_lfib() {
    const string FEC_R0_TO_R3 = "R0->R3";
    INGRESS_LFIB[0][FEC_R0_TO_R3] = LFIBEntry(PUSH, 300, 2, FEC_R0_TO_R3);
    GLOBAL_LFIB[2][300] = LFIBEntry(SWAP, 777, 3);
    GLOBAL_LFIB[3][777] = LFIBEntry(POP, -1, -1);
}

struct Packet {
    int source;
    int destination;
    int label;
    Packet(int src, int dest) : source(src), destination(dest), label(0) {}
};

void forward_packet(int router_idx, Packet& p);

void forward_packet(int router_idx, Packet& p) {
    if (router_idx == 0) {
        string fec = ROUTER_NAMES[p.source] + "->" + ROUTER_NAMES[p.destination];
        if (INGRESS_LFIB[router_idx].count(fec)) {
            const LFIBEntry& entry = INGRESS_LFIB[router_idx].at(fec);
            p.label = entry.out_label;
            cout << "[" << ROUTER_NAMES[router_idx] << "] Packet for " << ROUTER_NAMES[p.destination]
                 << " (FEC: " << entry.fec << "). Pushing Label " << p.label
                 << ". Sending to " << ROUTER_NAMES[entry.next_hop] << "." << endl;
            forward_packet(entry.next_hop, p);
        } else {
            cout << "[" << ROUTER_NAMES[router_idx] << "] No FEC match for destination " << ROUTER_NAMES[p.destination] << ". Dropping." << endl;
        }
    } else if (router_idx == 2) {
        if (GLOBAL_LFIB[router_idx].count(p.label)) {
            const LFIBEntry& entry = GLOBAL_LFIB[router_idx].at(p.label);
            int in_label = p.label;
            p.label = entry.out_label;
            cout << "[" << ROUTER_NAMES[router_idx] << "] Received packet with In-Label " << in_label
                 << ". Swapping for Out-Label " << p.label
                 << ". Sending to " << ROUTER_NAMES[entry.next_hop] << "." << endl;
            forward_packet(entry.next_hop, p);
        } else {
            cout << "[" << ROUTER_NAMES[router_idx] << "] No LFIB entry for In-Label " << p.label << ". Dropping." << endl;
        }
    } else if (router_idx == 3) {
        if (GLOBAL_LFIB[router_idx].count(p.label)) {
            const LFIBEntry& entry = GLOBAL_LFIB[router_idx].at(p.label);
            int in_label = p.label;
            p.label = 0;
            cout << "[" << ROUTER_NAMES[router_idx] << "] Received packet with In-Label " << in_label
                 << ". Popping label. Packet delivered." << endl;
        } else {
            cout << "[" << ROUTER_NAMES[router_idx] << "] No LFIB entry for In-Label " << p.label << ". Dropping." << endl;
        }
    } else {
        cout << "[" << ROUTER_NAMES[router_idx] << "] Router behavior not implemented. Dropping." << endl;
    }
}

int main() {
    cout << fixed << left;
    cout << "--- Task 2: Routing Information Base (RIB) Computation ---" << endl << endl;
    for (int i = 0; i < NUM_ROUTERS; ++i) {
        GLOBAL_RIB[i] = dijkstra(i);
        print_routing_table(i, GLOBAL_RIB[i]);
    }
    setup_lfib();
    cout << "--- Task 5: MPLS Packet Forwarding Simulation ---" << endl << endl;
    Packet p(0, 3);
    cout << "Simulating packet from R0 to R3..." << endl;
    forward_packet(p.source, p);
    cout << endl;
    return 0;
}
