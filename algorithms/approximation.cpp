#include "approximation.hpp"
#include "maximum_flow.hpp"

#include <set>
#include <algorithm>
#include <limits>
#include <glpk.h>
#include <iostream>

using namespace std;

Approximation::Approximation(Graph graph, const vector<NodeId> &terminals)
    : graph(graph), terminals(terminals),
      in_region(terminals.size(), vector<bool>(graph.number_of_nodes(), false)),
      region_nodes(terminals.size(), vector<NodeId>()),
      node_state(graph.number_of_nodes(), OUTSIDE)
{
}

//Solves dual LP and Returns optimal soluiton
optional<DualLPSolution> Approximation::calculate_optimal_solution(DualLPSolution &sol, int time_limit_ms)
{
    // Build the polynomial-size dual LP and extract the node lengths d and
    // terminal-to-node distances y from the primal solution returned by GLPK.
    long long num_edges = graph.number_of_edges();
    int num_terminals = terminals.size();
    int num_nodes = graph.number_of_nodes();

    vector<int> d_col(num_nodes, -1);
    vector<vector<int>> y_col(num_nodes, vector<int>(num_terminals, -1));
    glp_prob *lp = glp_create_prob();
    glp_set_obj_dir(lp, GLP_MIN);

    int col = 0;
    // One d_v variable for each non-terminal node.
    for (NodeId v = 0; v < num_nodes; v++)
    {
        if (graph.get_node(v).terminal)
            continue;

        col++;
        glp_add_cols(lp, 1);

        d_col[v] = col;

        glp_set_col_bnds(lp, col, GLP_LO, 0.0, 0.0); // d_v >= 0
        glp_set_obj_coef(lp, col, graph.get_node(v).weight);
    }

    // One y[u][j] variable for each node/terminal pair. These represent the
    // distance from terminal s_j to node u in the dual reformulation.
    for (NodeId u = 0; u < num_nodes; u++)
    {
        for (int j = 0; j < num_terminals; j++)
        {
            col++;
            glp_add_cols(lp, 1);

            y_col[u][j] = col;

            glp_set_col_bnds(lp, col, GLP_LO, 0.0, 0.0); // y ≥ 0
            glp_set_obj_coef(lp, col, 0.0);
        }
    }

    long long row_count_ll = num_edges * num_terminals + num_terminals + 1LL * num_terminals * (num_terminals - 1);
    long long col_count_ll = (num_nodes - (long long)terminals.size()) + 1LL * num_nodes * num_terminals;

    // Skip instances whose LP would become too large for the configured setup.
    long long nnz_est_ll = 3LL * num_edges * num_terminals + num_terminals + 1LL * num_terminals * (num_terminals - 1);

    if (row_count_ll > 10000000LL || col_count_ll > 5000000LL || nnz_est_ll > 30000000LL)
    {
        timed_out = true; // or set a separate skipped flag if you want
        return nullopt;
    }

    // Store the LP matrix as sparse (row, column, value) triplets for GLPK.
    int row_count = static_cast<int>(row_count_ll);
    glp_add_rows(lp, row_count);

    vector<int> ia(1), ja(1);
    vector<double> ar(1);

    auto add = [&](int r, int c, double val)
    {
        ia.push_back(r);
        ja.push_back(c);
        ar.push_back(val);
    };

    int row = 0;

    // For each directed edge (u,v) and each terminal j, enforce
    // y[v][j] - y[u][j] <= d[v].
    for (const Edge &e : graph.get_edges())
    {
        if (!e.active)
            continue;

        NodeId u = e.src;
        NodeId v = e.trg;

        for (int j = 0; j < num_terminals; j++)
        {
            row++;
            glp_set_row_bnds(lp, row, GLP_UP, 0.0, 0.0);

            add(row, y_col[v][j], +1.0);
            add(row, y_col[u][j], -1.0);

            if (!graph.get_node(v).terminal)
            {
                add(row, d_col[v], -1.0);
            }
        }
    }

    // Fix each terminal to distance 0 from itself.
    for (int j = 0; j < num_terminals; j++)
    {
        row++;
        NodeId sj = terminals[j];

        glp_set_row_bnds(lp, row, GLP_FX, 0.0, 0.0);
        add(row, y_col[sj][j], 1.0);
    }

    // Enforce distance at least 1 between distinct terminals.
    for (int i = 0; i < num_terminals; i++)
    {
        for (int j = 0; j < num_terminals; j++)
        {
            if (i == j)
                continue;

            row++;
            NodeId si = terminals[i];

            glp_set_row_bnds(lp, row, GLP_LO, 1.0, 0.0);
            add(row, y_col[si][j], 1.0);
        }
    }

    glp_smcp params;
    glp_init_smcp(&params);
    params.tm_lim = time_limit_ms; // Set time limit in milliseconds
    params.msg_lev = GLP_MSG_ERR;

    glp_load_matrix(lp, ia.size() - 1, ia.data(), ja.data(), ar.data());
    int ret = glp_simplex(lp, &params);
    if (ret == GLP_ETMLIM)
    {
        timed_out = true;
        return nullopt; // return empty solution
    }
    // DualLPSolution sol;
    sol.d.assign(num_nodes, 0.0);
    sol.y.assign(num_nodes, vector<double>(num_terminals, 0.0));

    sol.objective_value = glp_get_obj_val(lp);

    // Read back the dual node lengths d_v.
    for (NodeId v = 0; v < num_nodes; v++)
    {
        if (!graph.get_node(v).terminal)
        {
            sol.d[v] = glp_get_col_prim(lp, d_col[v]);

            // cout << "d[" << v << "] = " << sol.d[v] << "\n";
        }
    }

    const double EPS = 1e-6;
    // Nodes at distance 0 from terminal j belong to the region of j.
    for (NodeId u = 0; u < num_nodes; u++)
    {
        for (int j = 0; j < num_terminals; j++)
        {
            sol.y[u][j] = glp_get_col_prim(lp, y_col[u][j]);

            if (sol.y[u][j] <= EPS)
            {
                in_region[j][u] = true;
                region_nodes[j].push_back(u);
                node_state[u] = IN_REGION;

                // cout << "y[" << u << "][" << j << "] = " << sol.y[u][j] << " (in region of terminal " << terminals[j] << ")\n";
            }
        }
    }

    glp_delete_prob(lp);
    return sol;
}

optional<vector<NodeId>> Approximation::run(int time_limit_ms)
{
    // If two terminals are adjacent, no valid node multiway cut exists because
    // terminals themselves are not allowed to be removed.
    for (const Edge &edge : graph.get_edges())
    {
        if (!edge.active)
            continue;

        NodeId u = edge.src;
        NodeId v = edge.trg;

        if (graph.get_node(u).terminal && graph.get_node(v).terminal)
        {
            cout << "Edge " << edge.id << " has its two ends in two different terminal sets." << endl;
            return nullopt;
        }
    }

    vector<NodeId> cutNodes;
    int num_terminals = terminals.size();
    int num_nodes = graph.number_of_nodes();

    // Solve the LP and obtain the region structure induced by the dual solution.
    DualLPSolution solution;
    optional<DualLPSolution> lp_solution = calculate_optimal_solution(solution, time_limit_ms);

    if (!lp_solution.has_value())
    {
        cout << "Failed to compute LP solution within time limit." << endl;
        timed_out = true;
        return nullopt;
    }

    // initialize boundary tracking structures
    in_boundary.assign(num_terminals, vector<bool>(num_nodes, false));
    boundary_count.assign(num_nodes, 0);
    first_owner.assign(num_nodes, -1);
    half_boundary_weight.assign(num_terminals, 0);
    in_M.assign(num_nodes, false);

    // For each terminal region, inspect outgoing neighbors and classify them as
    // boundary nodes. track whether a boundary node belongs to exactly one
    // region or is shared by multiple regions.
    for (int t = 0; t < num_terminals; t++)
    {
        NodeId terminal = terminals[t];

        vector<NodeId> regional_nodes = region_nodes[t];
        regional_nodes.push_back(terminal); // add terminal itself to its region

        for (NodeId region_node : regional_nodes)
        {

            vector<NodeId> neighbors = graph.get_neighboring_nodes(region_node);

            for (int i = 0; i < neighbors.size(); i++)
            {
                NodeId neighbor = neighbors[i];
                if (graph.get_node(neighbor).terminal || node_state[neighbor] == IN_REGION)
                    continue;
                else if (lp_solution.has_value() && lp_solution.value().d[neighbor] == 0.5 || node_state[neighbor] == OUTSIDE)
                {
                    // belongs to a unique boundary
                    node_state[neighbor] = UNIQUE_BOUNDARY;
                    in_boundary[t][neighbor] = true;
                    in_M[neighbor] = true;

                    first_owner[neighbor] = t;
                    boundary_count[neighbor] = 1;

                    half_boundary_weight[t] += graph.get_node(neighbor).weight;
                }
                else if (lp_solution.has_value() && lp_solution.value().d[neighbor] == 1.0 || node_state[neighbor] == UNIQUE_BOUNDARY && first_owner[neighbor] != t)
                {
                    // Belongs to multiple boundaries, so update state and adjust half boundary weight if it was previously counted as unique.
                    node_state[neighbor] = MULTI_BOUNDARY;
                    int owner = first_owner[neighbor];
                    half_boundary_weight[owner] -= graph.get_node(neighbor).weight;
                    boundary_count[neighbor]++;
                }
            }
        }
    }

    // identify highest weight half boundary nodes set
    int heaviest_half_boundary = -1;
    int max_half_boundary_weight = -1;

    // The approximation removes all boundary nodes except the heaviest unique
    // half-boundary.
    for (int i = 0; i < num_terminals; i++)
    {
        if (half_boundary_weight[i] > max_half_boundary_weight)
        {
            max_half_boundary_weight = half_boundary_weight[i];
            heaviest_half_boundary = i;
        }
    }

    // Construct the final node cut: union of all boundary nodes minus the
    // unique boundary of the chosen terminal.
    set<NodeId> cutSet;
    for (NodeId v = 0; v < num_nodes; v++)
    {
        if (!in_M[v])
            continue;

        // remove nodes that are uniquely in boundary of best_j
        if (boundary_count[v] == 1 && first_owner[v] == heaviest_half_boundary)
        {
            continue;
        }

        cutSet.insert(v);
    }

    // return union of all boundary nodes except the highest weighing halfboundary set.
    return vector<NodeId>(cutSet.begin(), cutSet.end());
}
