# Lab 5: MPLS Routing Simulation

## Overview

This lab simulates Multiprotocol Label Switching (MPLS), a forwarding mechanism that uses labels to make forwarding decisions instead of examining packet headers. MPLS is fundamental to modern network architectures for traffic engineering, VPN services, and fast packet forwarding.

## Objectives

- Understand MPLS architecture and label switching
- Implement label distribution and label switching paths (LSPs)
- Learn shortest path algorithms (Dijkstra) for routing
- Simulate network topology and routing tables
- Analyze path optimization using MPLS

## Files

### `mpls_simulation.cpp`
Comprehensive MPLS simulation implementation.

**Key Components:**
- Router network representation
- Label management system
- Label Distribution Protocol (LDP) simulation
- Path computation and label allocation
- Forwarding table management

## MPLS Concepts

### Labels and Label Switching

**Label Structure:**
```
32-bit Label
┌─────────────────┬──────┬─────────────────┐
│  Label Value    │ Exp  │ S │ TTL (8 bits) │
│  (20 bits)      │ (3)  │ (1)              │
└─────────────────┴──────┴─────────────────┘
```

- **Label Value**: Identifies the LSP (0-1048575)
- **Exp**: Class of Service (0-7)
- **S (Stack bit)**: Indicates bottom of label stack
- **TTL**: Time To Live (prevents loops)

### Label Switching Path (LSP)

**Definition:** A predetermined path through MPLS network from ingress to egress

**Path Components:**
1. **Ingress LER** (Label Edge Router): Adds labels to packets
2. **Intermediate LSRs** (Label Switch Routers): Swap labels based on LFIB
3. **Egress LER**: Removes labels

**Example LSP:**
```
Host A → R0 (Label 100) → R1 (Label 200) → R3 (Label Pop) → Host B
```

### Label Distribution Protocol (LDP)

**Process:**
1. Routers discover neighbors
2. Exchange prefix reachability information
3. Allocate labels for prefixes
4. Build forwarding tables

## Network Topology

### Topology Structure
```
    R0 --- R1
    |      |
   10     20
    |      |
    R2 --- R3
   20     10
```

### Distance Matrix
```
     R0  R1  R2  R3
R0 [  0  10  20  ∞ ]
R1 [ 10   0  ∞  20 ]
R2 [ 20  ∞   0  10 ]
R3 [ ∞  20  10   0 ]
```

## Data Structures

### Router Information Base (RIB)
Stores routing information:
```cpp
struct RIBEntry {
    int next_hop;      // Next router ID
    int cost;          // Path cost (hops or metric)
};
```

### Label Forwarding Information Base (LFIB)
Maps incoming labels to outgoing actions:
```
Incoming Label | Outgoing Label | Next Hop
    100        |      200       |    R1
    101        |      Pop       |    R3
```

### Routing Table
Maintains next hop information for all destinations

## Algorithms

### Dijkstra's Shortest Path Algorithm

**Purpose:** Find minimum-cost path from source to all destinations

**Algorithm:**
1. Initialize distances to all nodes as infinity, source as 0
2. Mark all nodes as unvisited
3. Repeat:
   - Select unvisited node with minimum distance
   - Mark as visited
   - Update distances to neighbors
   - Stop when destination reached or all nodes visited

**Time Complexity:** O(V²) with array, O((V+E) log V) with heap

**Pseudocode:**
```
DIJKSTRA(Graph G, Vertex s):
  for each vertex v:
    distance[v] = INFINITY
    parent[v] = NULL
  distance[s] = 0
  
  while unvisited vertices exist:
    u = unvisited vertex with min distance
    mark u as visited
    for each neighbor v of u:
      if distance[u] + weight(u,v) < distance[v]:
        distance[v] = distance[u] + weight(u,v)
        parent[v] = u
```

## Implementation Details

### Routing Table Computation
1. Run Dijkstra from each router
2. Build RIB with next hop and cost
3. Store in global routing table

### Label Allocation
1. LDP discovers neighbors via hello messages
2. Allocates labels for each prefix
3. Creates LSP mappings

### Forwarding Process
1. Ingress router: Add label based on destination
2. Intermediate routers: Swap label, forward via LFIB
3. Egress router: Pop label, use standard IP forwarding

## Compilation

```bash
# Standard C++11 compiler
g++ -std=c++11 -O2 -o mpls mpls_simulation.cpp

# Or with clang
clang++ -std=c++11 -O2 -o mpls mpls_simulation.cpp

# With verbose output
g++ -std=c++11 -DDEBUG -o mpls mpls_simulation.cpp
```

## Execution

```bash
# Run simulation
./mpls

# Run with output redirection
./mpls > output.txt 2>&1
```

## Expected Output

### Global Routing Information Base (GLOBAL_RIB)

```
=== MPLS Network Simulation ===

Global Routing Information Base:
Router 0:
  Destination R0: Next Hop=-1, Cost=0 (Direct)
  Destination R1: Next Hop=1, Cost=10
  Destination R2: Next Hop=2, Cost=20
  Destination R3: Next Hop=1, Cost=30

Router 1:
  Destination R0: Next Hop=0, Cost=10
  Destination R1: Next Hop=-1, Cost=0 (Direct)
  Destination R2: Next Hop=3, Cost=30
  Destination R3: Next Hop=3, Cost=20

Router 2:
  Destination R0: Next Hop=0, Cost=20
  Destination R1: Next Hop=3, Cost=30
  Destination R2: Next Hop=-1, Cost=0 (Direct)
  Destination R3: Next Hop=3, Cost=10

Router 3:
  Destination R0: Next Hop=1, Cost=30
  Destination R1: Next Hop=1, Cost=20
  Destination R2: Next Hop=2, Cost=10
  Destination R3: Next Hop=-1, Cost=0 (Direct)
```

### Shortest Paths

```
Shortest Paths:
R0 → R1: 10 (Direct)
R0 → R2: 20 (Direct)
R0 → R3: 30 (R0→R1→R3)

R1 → R0: 10 (Direct)
R1 → R2: 30 (R1→R3→R2)
R1 → R3: 20 (Direct)

R2 → R0: 20 (Direct)
R2 → R1: 30 (R2→R3→R1)
R2 → R3: 10 (Direct)

R3 → R0: 30 (R3→R1→R0)
R3 → R1: 20 (Direct)
R3 → R2: 10 (Direct)
```

## Key Concepts

### MPLS Advantages

1. **Fast Forwarding**: Label lookup faster than IP header parsing
2. **Traffic Engineering**: Explicit path control
3. **VPN Support**: Create virtual networks over shared infrastructure
4. **QoS**: Assign different forwarding behaviors per label
5. **Seamless Integration**: Works with IPv4, IPv6, and other protocols

### MPLS vs Traditional IP Routing

| Aspect | IP | MPLS |
|--------|----|----|
| Forwarding | Header lookup | Label lookup |
| Speed | Medium | Fast |
| Complexity | Simple | Complex |
| Traffic Engineering | Limited | Excellent |
| VPN Support | No | Yes |

### Use Cases

1. **Provider Backbone Networks**: ISP core networks
2. **Traffic Engineering**: Optimize network utilization
3. **VPN Services**: Customer isolation
4. **Fast Rerouting**: Quick backup path activation
5. **QoS Implementation**: Service-level guarantees

## Simulation Scenarios

### Scenario 1: Single Source Routing
Compute and display LSPs from single source to all destinations

### Scenario 2: Multi-destination Routing
Show different paths for different destinations through network

### Scenario 3: Load Balancing
Distribute traffic across multiple paths

### Scenario 4: Link Failure Recovery
Reroute traffic when link fails

## Experiments to Try

1. **Add New Router**: Extend topology with Router 4
   ```cpp
   Add edges: R3-R4 (cost 15), R0-R4 (cost 25)
   ```

2. **Change Link Costs**: Simulate congestion
   ```cpp
   Increase cost of heavily used links
   ```

3. **Compare Paths**: Before/after cost modification

4. **Large Network**: Scale to 10+ routers

## Extensions

1. Implement constrained shortest path (CSPF)
2. Add label stack operations (push, pop, swap)
3. Simulate traffic distribution
4. Implement RSVP-TE (Resource Reservation Protocol)
5. Add fast rerouting mechanism
6. Implement equal-cost multi-path (ECMP)

## Troubleshooting

1. **Infinite Loop**: Check for uninitialized parent pointers
2. **Incorrect Paths**: Verify Dijkstra algorithm implementation
3. **Missing Entries**: Ensure all routers compute routing tables
4. **Output Issues**: Redirect stderr to see all output

## Related Concepts

- Link State Routing (OSPF, IS-IS)
- Distance Vector Routing (RIP)
- Constrained Shortest Path First (CSPF)
- Traffic Engineering
- Segment Routing (emerging MPLS evolution)

## Real-World MPLS Implementation

- **Cisco**: Forwarding Equivalence Classes (FEC)
- **Juniper**: MPLS support in MX series
- **RFC Standards**: RFC 3031, RFC 3032, RFC 5036

---

**Last Updated**: May 2026
