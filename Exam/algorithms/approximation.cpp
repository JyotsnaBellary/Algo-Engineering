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
{}

optional<DualLPSolution> Approximation::calculate_optimal_solution(DualLPSolution &sol, int time_limit_ms)
{
    // cout << "calculaing optimal solutio for dual LP" << endl;
    // cout << "terminals are: ";
    // for (NodeId t : terminals)
    // {
    //     cout << t << " ";
    // }
    // cout << endl;

    int num_nodes = graph.number_of_nodes();
    long long m = graph.number_of_edges();
    int num_terminals = terminals.size();

    int n = graph.number_of_nodes();
    int k = terminals.size();

    vector<int> d_col(n, -1);
    vector<vector<int>> y_col(n, vector<int>(k, -1));
    glp_prob *lp = glp_create_prob();
    glp_set_obj_dir(lp, GLP_MIN);

    int col = 0;
    // d_v variables
    for (NodeId v = 0; v < n; v++)
    {
        if (graph.get_node(v).terminal)
            continue;

        col++;
        glp_add_cols(lp, 1);

        d_col[v] = col;

        glp_set_col_bnds(lp, col, GLP_LO, 0.0, 0.0); // d_v >= 0
        // glp_set_col_bnds(lp, col, GLP_DB, 0.0, 1.0); // 0 ≤ d_v ≤ 1
        glp_set_obj_coef(lp, col, graph.get_node(v).weight);
    }

    // y[u][j] variables
    for (NodeId u = 0; u < n; u++)
    {
        for (int j = 0; j < k; j++)
        {
            col++;
            glp_add_cols(lp, 1);

            y_col[u][j] = col;

            glp_set_col_bnds(lp, col, GLP_LO, 0.0, 0.0); // y ≥ 0
            glp_set_obj_coef(lp, col, 0.0);
        }
    }

    // int row_count = graph.number_of_edges() * k + k + k * (k - 1);
long long row_count_ll = m * k + k + 1LL * k * (k - 1);
long long col_count_ll = (n - (long long)terminals.size()) + 1LL * n * k;

// rough estimate: each edge/terminal row contributes about 2-3 coefficients
long long nnz_est_ll = 3LL * m * k + k + 1LL * k * (k - 1);

if (row_count_ll > 10000000LL || col_count_ll > 5000000LL || nnz_est_ll > 30000000LL) {
    timed_out = true;   // or set a separate skipped flag if you want
    return nullopt;
}

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

    for (const Edge &e : graph.get_edges())
    {
        if (!e.active)
            continue;

        NodeId u = e.src;
        NodeId v = e.trg;

        for (int j = 0; j < k; j++)
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

    for (int j = 0; j < k; j++)
    {
        row++;
        NodeId sj = terminals[j];

        glp_set_row_bnds(lp, row, GLP_FX, 0.0, 0.0);
        add(row, y_col[sj][j], 1.0);
    }

    for (int i = 0; i < k; i++)
    {
        for (int j = 0; j < k; j++)
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
    if(ret == GLP_ETMLIM) {
        // cout << "LP solver reached time limit." << endl;
        timed_out = true;
        return nullopt; //return empty solution 
    } 
    // DualLPSolution sol;
    sol.d.assign(n, 0.0);
    sol.y.assign(n, vector<double>(k, 0.0));

    sol.objective_value = glp_get_obj_val(lp);

    // d values
    for (NodeId v = 0; v < n; v++)
    {
        if (!graph.get_node(v).terminal)
        {
            sol.d[v] = glp_get_col_prim(lp, d_col[v]);
            // cout << "d[" << v << "] = " << sol.d[v] << endl;
        }
    }

    const double EPS = 1e-6;
    // y values
    // vector<NodeId> with_one;
    for (NodeId u = 0; u < n; u++)
    {
        for (int j = 0; j < k; j++)
        {
            sol.y[u][j] = glp_get_col_prim(lp, y_col[u][j]);

            if (sol.y[u][j] <= EPS)
            {
                in_region[j][u] = true;
                region_nodes[j].push_back(u);
                node_state[u] = IN_REGION;
            }
        }
    }

    glp_delete_prob(lp);
    return sol;
}

optional<vector<NodeId>> Approximation::run(int time_limit_ms)
{
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

    // compute LP solution and get the optimal value
    DualLPSolution solution;
    optional<DualLPSolution> lp_solution = calculate_optimal_solution(solution, time_limit_ms);

    if (!lp_solution.has_value())
    {
        cout << "Failed to compute LP solution within time limit." << endl;
        timed_out = true;
        return nullopt;
    }

    
    in_boundary.assign(num_terminals, vector<bool>(num_nodes, false));
    boundary_count.assign(num_nodes, 0);
    first_owner.assign(num_nodes, -1);
    half_boundary_weight.assign(num_terminals, 0);
    in_M.assign(num_nodes, false);

    // for each terminal, mark unique and non unique boundry
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
                if (graph.get_node(neighbor).terminal  || node_state[neighbor] == IN_REGION)
                    continue;
                else if (lp_solution.has_value() && lp_solution.value().d[neighbor] == 0.5 || node_state[neighbor] == OUTSIDE)
                {
                    node_state[neighbor] = UNIQUE_BOUNDARY;
                    in_boundary[t][neighbor] = true;
                    in_M[neighbor] = true;

                    first_owner[neighbor] = t;
                    boundary_count[neighbor] = 1;

                    half_boundary_weight[t] += graph.get_node(neighbor).weight;
                }
                else if (lp_solution.has_value() && lp_solution.value().d[neighbor] == 1.0 && node_state[neighbor] == UNIQUE_BOUNDARY && first_owner[neighbor] != t)
                {
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

    for (int i = 0; i < num_terminals; i++)
    {
        if (half_boundary_weight[i] > max_half_boundary_weight)
        {
            max_half_boundary_weight = half_boundary_weight[i];
            heaviest_half_boundary = i;
        }
    }
    
    // compute  Output ⋃ Γ(Si) − Γ1/2(Sj)
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
