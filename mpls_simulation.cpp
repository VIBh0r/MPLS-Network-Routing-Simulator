// MPLS Label Forwarding Simulation
// Implements Task 1-5 of the assignment:
//   Task 1: represent the fixed 4-router topology
//   Task 2: compute per-router routing tables (RIB) with Dijkstra's algorithm
//   Task 3: define the single FEC used for this simulation ("R0 -> R3" traffic)
//   Task 4: build the Label Forwarding Information Base (LFIB) for the FEC's LSP
//   Task 5: simulate one packet being forwarded through that LSP

#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <map>
using namespace std;

// ---------------------------------------------------------------------------
// Task 1: Network topology
// ---------------------------------------------------------------------------
const int NUM_ROUTERS = 4;
const vector<string> ROUTER_NAMES = {"R0", "R1", "R2", "R3"};
const int INF = numeric_limits<int>::max();

// Fixed topology from the assignment: R0<->R1 (10), R0<->R2 (20),
// R1<->R3 (20), R2<->R3 (10). R0<->R3 and R1<->R2 have no direct link.
const vector<vector<int>> TOPOLOGY = {
    {0,   10,  20,  INF},
    {10,  0,   INF, 20},
    {20,  INF, 0,   10},
    {INF, 20,  10,  0}
};

// ---------------------------------------------------------------------------
// Task 2: Routing Information Base (RIB)
// ---------------------------------------------------------------------------
struct RIBEntry {
    int next_hop;
    int cost;
    RIBEntry() : next_hop(-1), cost(INF) {}
    RIBEntry(int nh, int c) : next_hop(nh), cost(c) {}
};

// GLOBAL_RIB[router][destination] = how that router forwards toward destination.
map<int, vector<RIBEntry>> GLOBAL_RIB;

// Runs Dijkstra's algorithm from source_router_idx and returns the resulting
// routing table (next hop + total cost to every other router).
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
        if (u == -1) break; // remaining routers are unreachable

        visited[u] = true;
        for (int v = 0; v < NUM_ROUTERS; ++v) {
            int weight = TOPOLOGY[u][v];
            if (visited[v] || weight == INF || dist[u] == INF) continue;
            // Use "<=" (not strict "<") so that when two paths tie on cost,
            // the path discovered through the later-processed neighbor wins.
            // This keeps the RIB consistent with the FEC's LSP built in Task 4.
            long long candidate = (long long)dist[u] + weight;
            if (candidate <= dist[v]) {
                dist[v] = (int)candidate;
                parent[v] = u;
            }
        }
    }

    // Convert parent pointers into a "next hop" per destination: walk
    // backwards from the destination until reaching the source's direct
    // neighbor on that path.
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

// Prints one router's routing table in the format required by the assignment:
//   Routing Table for Router R<id>:
//   Destination | Next Hop | Total Cost
//   ------------------------------------
//    R<dest> | R<next> | <cost>
void print_routing_table(int router_idx, const vector<RIBEntry>& rib_entries) {
    cout << "Routing Table for Router " << ROUTER_NAMES[router_idx] << ":" << endl;
    cout << "Destination | Next Hop | Total Cost" << endl;
    cout << "------------------------------------" << endl;
    for (int dest = 0; dest < NUM_ROUTERS; ++dest) {
        if (dest == router_idx) continue;
        const RIBEntry& entry = rib_entries[dest];
        string next_hop_name = (entry.next_hop != -1) ? ROUTER_NAMES[entry.next_hop] : "N/A";
        string cost_str = (entry.cost != INF) ? to_string(entry.cost) : "INF";
        cout << " " << ROUTER_NAMES[dest] << " | " << next_hop_name << " | " << cost_str << endl;
    }
    cout << endl;
}

// ---------------------------------------------------------------------------
// Task 3 / Task 4: Forwarding Equivalence Class (FEC) and LFIB
// ---------------------------------------------------------------------------
// Task 3: this simulation covers a single FEC - "all traffic originating from
// R0 and destined for R3" - identified here by the string "R0->R3".

enum LabelOperation { PUSH, SWAP, POP };

struct LFIBEntry {
    LabelOperation operation;
    int out_label;
    int next_hop;
    string fec; // only meaningful for ingress (PUSH) entries
    LFIBEntry() : operation(PUSH), out_label(-1), next_hop(-1), fec("") {}
    LFIBEntry(LabelOperation op, int out_l, int nh, string f) : operation(op), out_label(out_l), next_hop(nh), fec(f) {}
    LFIBEntry(LabelOperation op, int out_l, int nh) : operation(op), out_label(out_l), next_hop(nh), fec("") {}
};

// GLOBAL_LFIB[router][incoming_label]  -> used by transit/egress routers.
// INGRESS_LFIB[router][fec]            -> used by the ingress router, which
// has no incoming label yet and must classify the packet by FEC instead.
map<int, map<int, LFIBEntry>> GLOBAL_LFIB;
map<int, map<string, LFIBEntry>> INGRESS_LFIB;

// Builds the LFIB for the R0->R3 FEC's Label Switched Path. Per the
// assignment, R3 (egress) allocates label 777 and advertises it to R2, which
// allocates label 300 and advertises it to R0 (labels are assigned by the
// downstream router and distributed upstream, e.g. via LDP - not simulated
// here, only the resulting tables are).
void setup_lfib() {
    const string FEC_R0_TO_R3 = "R0->R3";

    // Task 4 is built on top of Task 2: R0 pushes its first label toward
    // whichever router its own RIB says is the next hop for R3.
    int r0_next_hop_for_r3 = GLOBAL_RIB[0][3].next_hop;

    INGRESS_LFIB[0][FEC_R0_TO_R3] = LFIBEntry(PUSH, 300, r0_next_hop_for_r3, FEC_R0_TO_R3);
    GLOBAL_LFIB[2][300] = LFIBEntry(SWAP, 777, 3);
    GLOBAL_LFIB[3][777] = LFIBEntry(POP, -1, -1);
}

// ---------------------------------------------------------------------------
// Task 5: Packet forwarding simulation
// ---------------------------------------------------------------------------
struct Packet {
    int source;
    int destination;
    int label; // 0 = no label (plain IP)
    Packet(int src, int dest) : source(src), destination(dest), label(0) {}
};

void forward_packet(int router_idx, Packet& p);

void forward_packet(int router_idx, Packet& p) {
    if (router_idx == 0) {
        // Ingress: classify the packet into a FEC and push the first label.
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
        // Transit: ignore the IP header entirely, look up only the label.
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
        // Egress: pop the label and deliver the packet locally.
        if (GLOBAL_LFIB[router_idx].count(p.label)) {
            int in_label = p.label;
            p.label = 0;
            cout << "[" << ROUTER_NAMES[router_idx] << "] Received packet with In-Label " << in_label
                 << ". Popping label. Packet delivered." << endl;
        } else {
            cout << "[" << ROUTER_NAMES[router_idx] << "] No LFIB entry for In-Label " << p.label << ". Dropping." << endl;
        }
    } else {
        // No LFIB/FEC entry is configured for this router in this simulation.
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
