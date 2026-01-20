package com.ciel.microservices.dispatch_engine_service.mod_route;

public class AlgorithmParameters {
    public int nb_iterations;
    public int time_limit;
    public int thread_count;

    public double shaw_phi_distance;
    public double shaw_chi_time;
    public double shaw_psi_capacity;

    public int shaw_removal_p;
    public int worst_removal_p;

    public double simulated_annealing_start_temp_control_w;
    public double simulated_annealing_cooling_rate_c;
    public double adaptive_weight_adj_d1;
    public double adaptive_weight_adj_d2;
    public double adaptive_weight_adj_d3;
    public double adaptive_weight_dacay_r;

    public double insertion_objective_noise_n;
    public double removal_req_iteration_control_e;

    public double delaytime_penalty;
    public double waittime_penalty;

    public int seed;
    public boolean enable_missing_solution;
    public boolean skip_remove_route;
    public int unfeasible_delaytime;
}
