#include "lib_modroute.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(mod_route, m) {
    m.doc() = "MOD Route Optimization Module";
    py::class_<Location>(m, "Location")
        .def(py::init<>())
        .def(py::init<double, double, int>(), py::arg("lng"), py::arg("lat"), py::arg("direction"))
        .def(py::init<double, double, int, std::string>(), py::arg("lng"), py::arg("lat"), py::arg("direction"), py::arg("station_id"))
        .def_readwrite("lng", &Location::lng)
        .def_readwrite("lat", &Location::lat)
        .def_readwrite("waypoint_id", &Location::waypoint_id)
        .def_readwrite("station_id", &Location::station_id)
        .def_readwrite("direction", &Location::direction)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const Location &loc) {
                return py::make_tuple(loc.lng, loc.lat, loc.direction, loc.station_id, loc.waypoint_id);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 5) {
                    throw std::runtime_error("Invalid state for Location");
                }
                auto _loc = Location(t[0].cast<double>(), t[1].cast<double>(), t[2].cast<int>(), t[3].cast<std::string>());
                _loc.waypoint_id = t[4].cast<int>();
                return _loc;
            }
        ));
    py::class_<VehicleLocation>(m, "VehicleLocation")
        .def(py::init<>())
        .def(py::init<std::string, int>(), py::arg("supply_idx"), py::arg("capacity"))
        .def_readwrite("supply_idx", &VehicleLocation::supplyIdx)
        .def_readwrite("capacity", &VehicleLocation::capacity)
        .def_readwrite("location", &VehicleLocation::location)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const VehicleLocation &vl) {
                return py::make_tuple(vl.supplyIdx, vl.capacity, vl.location);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 3) {
                    throw std::runtime_error("Invalid state for VehicleLocation");
                }
                auto _vl = VehicleLocation(t[0].cast<std::string>(), t[1].cast<int>());
                _vl.location = t[2].cast<Location>();
                return _vl;
            }
        ));
    py::class_<OnboardDemand>(m, "OnboardDemand")
        .def(py::init<>())
        .def(py::init<std::string, std::string, int>(), py::arg("id"), py::arg("supply_idx"), py::arg("demand"))
        .def_readwrite("id", &OnboardDemand::id)
        .def_readwrite("supply_idx", &OnboardDemand::supplyIdx)
        .def_readwrite("demand", &OnboardDemand::demand)
        .def_readwrite("destination_loc", &OnboardDemand::destinationLoc)
        .def_readwrite("eta_to_destination", &OnboardDemand::etaToDestination)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const OnboardDemand &od) {
                return py::make_tuple(od.id, od.supplyIdx, od.demand, od.destinationLoc, od.etaToDestination);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 5) {
                    throw std::runtime_error("Invalid state for OnboardDemand");
                }
                auto _od = OnboardDemand(t[0].cast<std::string>(), t[1].cast<std::string>(), t[2].cast<int>());
                _od.destinationLoc = t[3].cast<Location>();
                _od.etaToDestination = t[4].cast<std::array<int, 2>>();
                return _od;
            }
        ));
    py::class_<NewDemand>(m, "NewDemand")
        .def(py::init<>())
        .def(py::init<std::string, int>(), py::arg("id"), py::arg("demand"))
        .def_readwrite("id", &NewDemand::id)
        .def_readwrite("demand", &NewDemand::demand)
        .def_readwrite("start_loc", &NewDemand::startLoc)
        .def_readwrite("destination_loc", &NewDemand::destinationLoc)
        .def_readwrite("eta_to_start", &NewDemand::etaToStart)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const NewDemand &nd) {
                return py::make_tuple(nd.id, nd.demand, nd.startLoc, nd.destinationLoc, nd.etaToStart);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 5) {
                    throw std::runtime_error("Invalid state for NewDemand");
                }
                auto _nd = NewDemand(t[0].cast<std::string>(), t[1].cast<int>());
                _nd.startLoc = t[2].cast<Location>();
                _nd.destinationLoc = t[3].cast<Location>();
                _nd.etaToStart = t[4].cast<std::array<int, 2>>();
                return _nd;
            }
        ));
    py::class_<OnboardWaitingDemand>(m, "OnboardWaitingDemand")
        .def(py::init<>())
        .def(py::init<std::string, std::string, int>(), py::arg("id"), py::arg("supply_idx"), py::arg("demand"))
        .def_readwrite("id", &OnboardWaitingDemand::id)
        .def_readwrite("supply_idx", &OnboardWaitingDemand::supplyIdx)
        .def_readwrite("demand", &OnboardWaitingDemand::demand)
        .def_readwrite("start_loc", &OnboardWaitingDemand::startLoc)
        .def_readwrite("destination_loc", &OnboardWaitingDemand::destinationLoc)
        .def_readwrite("eta_to_start", &OnboardWaitingDemand::etaToStart)
        .def_readwrite("eta_to_destination", &OnboardWaitingDemand::etaToDestination)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const OnboardWaitingDemand &owd) {
                return py::make_tuple(owd.id, owd.supplyIdx, owd.demand, owd.startLoc, owd.destinationLoc, owd.etaToStart, owd.etaToDestination);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 7) {
                    throw std::runtime_error("Invalid state for OnboardWaitingDemand");
                }
                auto _owd = OnboardWaitingDemand(t[0].cast<std::string>(), t[1].cast<std::string>(), t[2].cast<int>());
                _owd.startLoc = t[3].cast<Location>();
                _owd.destinationLoc = t[4].cast<Location>();
                _owd.etaToStart = t[5].cast<std::array<int, 2>>();
                _owd.etaToDestination = t[6].cast<std::array<int, 2>>();
                return _owd;
            }
        ));
    py::class_<VehicleAssigned>(m, "VehicleAssigned")
        .def(py::init<>())
        .def(py::init<std::string>(), py::arg("supply_idx"))
        .def_readwrite("supply_idx", &VehicleAssigned::supplyIdx)
        .def_readwrite("route_order", &VehicleAssigned::routeOrder)
        .def_readwrite("route_times", &VehicleAssigned::routeTimes)
        .def_readwrite("route_distances", &VehicleAssigned::routeDistances)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const VehicleAssigned &va) {
                return py::make_tuple(va.supplyIdx, va.routeOrder, va.routeTimes, va.routeDistances);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 4) {
                    throw std::runtime_error("Invalid state for VehicleAssigned");
                }
                auto _va = VehicleAssigned(t[0].cast<std::string>());
                _va.routeOrder = t[1].cast<std::vector<std::pair<std::string, int>>>();
                _va.routeTimes = t[2].cast<std::vector<int64_t>>();
                _va.routeDistances = t[3].cast<std::vector<int64_t>>();
                return _va;
            }
        ));
    py::enum_<OptimizeType>(m, "OptimizeType")
        .value("Time", OptimizeType::Time)
        .value("Distance", OptimizeType::Distance)
        .value("Co2", OptimizeType::Co2)
        .export_values();
    py::class_<ModRequest>(m, "ModRequest")
        .def(py::init<>())
        .def_readwrite("vehicle_locs", &ModRequest::vehicleLocs)
        .def_readwrite("onboard_demands", &ModRequest::onboardDemands)
        .def_readwrite("onboard_waiting_demands", &ModRequest::onboardWaitingDemands)
        .def_readwrite("new_demands", &ModRequest::newDemands)
        .def_readwrite("assigned", &ModRequest::assigned)
        .def_readwrite("optimize_type", &ModRequest::optimizeType)
        .def_readwrite("max_solutions", &ModRequest::maxSolutions)
        .def_readwrite("loc_hash", &ModRequest::locHash)
        .def_readwrite("date_time", &ModRequest::dateTime)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const ModRequest &mr) {
                return py::make_tuple(mr.vehicleLocs, mr.onboardDemands, mr.onboardWaitingDemands, mr.newDemands, mr.assigned, mr.optimizeType, mr.maxSolutions, mr.locHash, mr.dateTime);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 9) {
                    throw std::runtime_error("Invalid state for ModRequest");
                }
                ModRequest _mr;
                _mr.vehicleLocs = t[0].cast<std::vector<VehicleLocation>>();
                _mr.onboardDemands = t[1].cast<std::vector<OnboardDemand>>();
                _mr.onboardWaitingDemands = t[2].cast<std::vector<OnboardWaitingDemand>>();
                _mr.newDemands = t[3].cast<std::vector<NewDemand>>();
                _mr.assigned = t[4].cast<std::vector<VehicleAssigned>>();
                _mr.optimizeType = t[5].cast<OptimizeType>();
                _mr.maxSolutions = t[6].cast<int>();
                _mr.locHash = t[7].cast<std::string>();
                _mr.dateTime = t[8].cast<std::optional<std::string>>();
                return _mr;
            }
        ));
    py::enum_<RouteType>(m, "RouteType")
        .value("OSRM", RouteType::ROUTE_OSRM)
        .value("VALHALLA", RouteType::ROUTE_VALHALLA)
        .export_values();
    py::class_<ModRoute>(m, "ModRoute")
        .def(py::init<>())
        .def_readwrite("id", &ModRoute::id)
        .def_readwrite("demand", &ModRoute::demand)
        .def_readwrite("location", &ModRoute::location)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const ModRoute &mr) {
                return py::make_tuple(mr.id, mr.demand, mr.location);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 3) {
                    throw std::runtime_error("Invalid state for ModRoute");
                }
                ModRoute _mr;
                _mr.id = t[0].cast<std::string>();
                _mr.demand = t[1].cast<int>();
                _mr.location = t[2].cast<Location>();
                return _mr;
            }
        ));
    py::class_<ModVehicleRoute>(m, "ModVehicleRoute")
        .def(py::init<>())
        .def_readwrite("supply_idx", &ModVehicleRoute::supply_idx)
        .def_readwrite("routes", &ModVehicleRoute::routes)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const ModVehicleRoute &mvr) {
                return py::make_tuple(mvr.supply_idx, mvr.routes);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 2) {
                    throw std::runtime_error("Invalid state for ModVehicleRoute");
                }
                ModVehicleRoute _mvr;
                _mvr.supply_idx = t[0].cast<std::string>();
                _mvr.routes = t[1].cast<std::vector<ModRoute>>();
                return _mvr;
            }
        ));
    py::class_<ModDispatchSolution>(m, "ModDispatchSolution")
        .def(py::init<>())
        .def_readwrite("vehicle_routes", &ModDispatchSolution::vehicle_routes)
        .def_readwrite("missing", &ModDispatchSolution::missing)
        .def_readwrite("unacceptables", &ModDispatchSolution::unacceptables)
        .def_readwrite("total_distance", &ModDispatchSolution::total_distance)
        .def_readwrite("total_time", &ModDispatchSolution::total_time)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const ModDispatchSolution &mds) {
                return py::make_tuple(mds.vehicle_routes, mds.missing, mds.unacceptables, mds.total_distance, mds.total_time);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 5) {
                    throw std::runtime_error("Invalid state for ModDispatchSolution");
                }
                ModDispatchSolution _mds;
                _mds.vehicle_routes = t[0].cast<std::vector<ModVehicleRoute>>();
                _mds.missing = t[1].cast<std::vector<ModRoute>>();
                _mds.unacceptables = t[2].cast<std::vector<ModRoute>>();
                _mds.total_distance = t[3].cast<int64_t>();
                _mds.total_time = t[4].cast<int64_t>();
                return _mds;
            }
        ));
        
    py::class_<ModRouteConfiguration>(m, "ModRouteConfiguration")
        .def(py::init<>())
        .def_readwrite("max_duration", &ModRouteConfiguration::nMaxDuration)
        .def_readwrite("bypass_ratio", &ModRouteConfiguration::nBypassRatio)
        .def_readwrite("service_time", &ModRouteConfiguration::nServiceTime)
        .def_readwrite("acceptable_buffer", &ModRouteConfiguration::nAcceptableBuffer)
        .def_readwrite("log_request", &ModRouteConfiguration::bLogRequest)
        .def_readwrite("cache_expiration_time", &ModRouteConfiguration::nCacheExpirationTime)
        .def_readwrite("solution_limit", &ModRouteConfiguration::nSolutionLimit)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const ModRouteConfiguration &conf) {
                return py::make_tuple(conf.nMaxDuration, conf.nBypassRatio, conf.nServiceTime, conf.nAcceptableBuffer, conf.bLogRequest, conf.nCacheExpirationTime, conf.nSolutionLimit);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 7) {
                    throw std::runtime_error("Invalid state for ModRouteConfiguration");
                }
                ModRouteConfiguration _conf;
                _conf.nMaxDuration = t[0].cast<int>();
                _conf.nBypassRatio = t[1].cast<int>();
                _conf.nServiceTime = t[2].cast<int>();
                _conf.nAcceptableBuffer = t[3].cast<int>();
                _conf.bLogRequest = t[4].cast<bool>();
                _conf.nCacheExpirationTime = t[5].cast<int>();
                _conf.nSolutionLimit = t[6].cast<int>();
                return _conf;
            }
        ));
    py::class_<AlgorithmParameters>(m, "AlgorithmParameters")
        .def(py::init<>())
        .def_readwrite("nb_iterations", &AlgorithmParameters::nb_iterations)
        .def_readwrite("time_limit", &AlgorithmParameters::time_limit)
        .def_readwrite("thread_count", &AlgorithmParameters::thread_count)
        .def_readwrite("shaw_phi_distance", &AlgorithmParameters::shaw_phi_distance)
        .def_readwrite("shaw_chi_time", &AlgorithmParameters::shaw_chi_time)
        .def_readwrite("shaw_psi_capacity", &AlgorithmParameters::shaw_psi_capacity)
        .def_readwrite("shaw_removal_p", &AlgorithmParameters::shaw_removal_p)
        .def_readwrite("worst_removal_p", &AlgorithmParameters::worst_removal_p)
        .def_readwrite("simulated_annealing_start_temp_control_w", &AlgorithmParameters::simulated_annealing_start_temp_control_w)
        .def_readwrite("simulated_annealing_cooling_rate_c", &AlgorithmParameters::simulated_annealing_cooling_rate_c)
        .def_readwrite("adaptive_weight_adj_d1", &AlgorithmParameters::adaptive_weight_adj_d1)
        .def_readwrite("adaptive_weight_adj_d2", &AlgorithmParameters::adaptive_weight_adj_d2)
        .def_readwrite("adaptive_weight_adj_d3", &AlgorithmParameters::adaptive_weight_adj_d3)
        .def_readwrite("adaptive_weight_dacay_r", &AlgorithmParameters::adaptive_weight_dacay_r)
        .def_readwrite("insertion_objective_noise_n", &AlgorithmParameters::insertion_objective_noise_n)
        .def_readwrite("removal_req_iteration_control_e", &AlgorithmParameters::removal_req_iteration_control_e)
        .def_readwrite("delaytime_penalty", &AlgorithmParameters::delaytime_penalty)
        .def_readwrite("waittime_penalty", &AlgorithmParameters::waittime_penalty)
        .def_readwrite("seed", &AlgorithmParameters::seed)
        .def_readwrite("enable_missing_solution", &AlgorithmParameters::enable_missing_solution)
        .def_readwrite("skip_remove_route", &AlgorithmParameters::skip_remove_route)
        .def_readwrite("unfeasible_delaytime", &AlgorithmParameters::unfeasible_delaytime)
        .def(py::pickle(
            /* __getstate__ (객체를 직렬화할 때 호출) */
            [](const AlgorithmParameters &ap) {
                return py::make_tuple(ap.nb_iterations, ap.time_limit, ap.thread_count,
                    ap.shaw_phi_distance, ap.shaw_chi_time, ap.shaw_psi_capacity,
                    ap.shaw_removal_p, ap.worst_removal_p,
                    ap.simulated_annealing_start_temp_control_w, ap.simulated_annealing_cooling_rate_c, ap.adaptive_weight_adj_d1, ap.adaptive_weight_adj_d2, ap.adaptive_weight_adj_d3, ap.adaptive_weight_dacay_r,
                    ap.insertion_objective_noise_n, ap.removal_req_iteration_control_e,
                    ap.delaytime_penalty, ap.waittime_penalty,
                    ap.seed, ap.enable_missing_solution, ap.skip_remove_route, ap.unfeasible_delaytime);
            },
            /* __setstate__ (pickup 된 state로부터 객체를 복원할 때 호출) */
            [](py::tuple t) {
                if (t.size() != 22) {
                    throw std::runtime_error("Invalid state for AlgorithmParameters");
                }
                AlgorithmParameters _ap;
                _ap.nb_iterations = t[0].cast<int>();
                _ap.time_limit = t[1].cast<int>();
                _ap.thread_count = t[2].cast<int>();
                _ap.shaw_phi_distance = t[3].cast<double>();
                _ap.shaw_chi_time = t[4].cast<double>();
                _ap.shaw_psi_capacity = t[5].cast<double>();
                _ap.shaw_removal_p = t[6].cast<double>();
                _ap.worst_removal_p = t[7].cast<double>();
                _ap.simulated_annealing_start_temp_control_w = t[8].cast<double>();
                _ap.simulated_annealing_cooling_rate_c = t[9].cast<double>();
                _ap.adaptive_weight_adj_d1 = t[10].cast<double>();
                _ap.adaptive_weight_adj_d2 = t[11].cast<double>();
                _ap.adaptive_weight_adj_d3 = t[12].cast<double>();
                _ap.adaptive_weight_dacay_r = t[13].cast<double>();
                _ap.insertion_objective_noise_n = t[14].cast<double>();
                _ap.removal_req_iteration_control_e = t[15].cast<double>();
                _ap.delaytime_penalty = t[16].cast<double>();
                _ap.waittime_penalty = t[17].cast<double>();
                _ap.seed = t[18].cast<int>();
                _ap.enable_missing_solution = t[19].cast<bool>();
                _ap.skip_remove_route = t[20].cast<bool>();
                _ap.unfeasible_delaytime = t[21].cast<int>();
                return _ap;
            }
        ));

    m.def("default_mod_configuraiton", &default_mod_configuraiton, "return default MOD route configuration");
    m.def("default_algorithm_parameters", &default_algorithm_parameters, "return default optimization algorithm parameters");
    m.def("run_optimize", &run_optimize, "run MOD route optimization", py::arg("mod_request"), py::arg("route_path"), py::arg("route_type"), py::arg("route_tasks") = 4, py::arg("cache_path"), py::arg("ap"), py::arg("conf"));
    m.def("clear_cache", &clear_cache, "clear cache");
}
