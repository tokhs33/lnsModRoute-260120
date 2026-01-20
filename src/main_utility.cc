#include <string>
#include <fstream>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <gason/gason.h>
#include <main_utility.h>

#define SOLUTION_INCLUDE_TIME

ModRequest parseRequest(const char *body)
{
    char *endptr;
    JsonValue value;
    JsonAllocator allocator;
    int status = jsonParse((char*) body, &endptr, &value, allocator);
    if (status != JSON_OK) {
        throw std::runtime_error("Fail to parse JSON request");
    }
    if (value.getTag() != JSON_OBJECT || !value.toNode()) {
        throw std::runtime_error("Invalid JSON request format");
    } 

    ModRequest request;
    request.maxSolutions = 0;
    request.dateTime = std::nullopt;

    for (auto item : value) {
        if (strcmp(item->key, "vehicle_locs") == 0) {
            if (item->value.getTag() == JSON_ARRAY && item->value.toNode()) {
                for (auto vehicle : item->value) {
                    if (vehicle->value.getTag() != JSON_OBJECT || !vehicle->value.toNode()) {
                        throw std::runtime_error("Invalid JSON request format");
                    }
                    VehicleLocation vehicleLoc;
                    vehicleLoc.location.waypoint_id = -1;
                    vehicleLoc.location.direction = -1;
                    for (auto vehicleItem : vehicle->value) {
                        if (strcmp(vehicleItem->key, "supply_idx") == 0) {
                            if (vehicleItem->value.getTag() == JSON_NUMBER) {
                                vehicleLoc.supplyIdx = std::to_string((int) vehicleItem->value.toNumber());
                            } else if (vehicleItem->value.getTag() == JSON_STRING) {
                                vehicleLoc.supplyIdx = vehicleItem->value.toString();
                            }
                        } else if (strcmp(vehicleItem->key, "capacity") == 0) {
                            vehicleLoc.capacity = (int) vehicleItem->value.toNumber();
                        } else if (strcmp(vehicleItem->key, "lat") == 0) {
                            vehicleLoc.location.lat = vehicleItem->value.toNumber();
                        } else if (strcmp(vehicleItem->key, "lng") == 0) {
                            vehicleLoc.location.lng = vehicleItem->value.toNumber();
                        } else if (strcmp(vehicleItem->key, "direction") == 0) {
                            if (vehicleItem->value.getTag() == JSON_NUMBER) {
                                vehicleLoc.location.direction = (int) vehicleItem->value.toNumber();
                            }
                        } else if (strcmp(vehicleItem->key, "operation_time") == 0) {
                            if (vehicleItem->value.getTag() == JSON_ARRAY && vehicleItem->value.toNode()) {
                                int i = 0;
                                for (auto op_time : vehicleItem->value) {
                                    vehicleLoc.operationTime[i++] = (int) op_time->value.toNumber();
                                    if (i >= 2) {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    request.vehicleLocs.push_back(vehicleLoc);
                }
            }
        } else if (strcmp(item->key, "onboard_demands") == 0) {
            if (item->value.getTag() == JSON_ARRAY && item->value.toNode()) {
                for (auto onboard : item->value) {
                    if (onboard->value.getTag() != JSON_OBJECT || !onboard->value.toNode()) {
                        throw std::runtime_error("Invalid JSON request format");
                    }
                    OnboardDemand onboardDemand;
                    onboardDemand.destinationLoc.waypoint_id = -1;
                    onboardDemand.destinationLoc.direction = -1;
                    onboardDemand.demand = 1;
                    for (auto onboardItem : onboard->value) {
                        if (strcmp(onboardItem->key, "id") == 0) {
                            if (onboardItem->value.getTag() == JSON_NUMBER) {
                                onboardDemand.id = std::to_string((int) onboardItem->value.toNumber());
                            } else if (onboardItem->value.getTag() == JSON_STRING) {
                                onboardDemand.id = onboardItem->value.toString();
                            }
                        } else if (strcmp(onboardItem->key, "supply_idx") == 0) {
                            if (onboardItem->value.getTag() == JSON_NUMBER) {
                                onboardDemand.supplyIdx = std::to_string((int) onboardItem->value.toNumber());
                            } else if (onboardItem->value.getTag() == JSON_STRING) {
                                onboardDemand.supplyIdx = onboardItem->value.toString();
                            }
                        } else if (strcmp(onboardItem->key, "demand") == 0) {
                            onboardDemand.demand = (int) onboardItem->value.toNumber();
                        } else if (strcmp(onboardItem->key, "destination_loc") == 0) {
                            if (onboardItem->value.getTag() == JSON_OBJECT && onboardItem->value.toNode()) {
                                for (auto loc : onboardItem->value) {
                                    if (strcmp(loc->key, "lat") == 0) {
                                        onboardDemand.destinationLoc.lat = loc->value.toNumber();
                                    } else if (strcmp(loc->key, "lng") == 0) {
                                        onboardDemand.destinationLoc.lng = loc->value.toNumber();
                                    } else if (strcmp(loc->key, "waypoint_id") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            onboardDemand.destinationLoc.waypoint_id = (int) loc->value.toNumber();
                                        }
                                    } else if (strcmp(loc->key, "station_id") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            onboardDemand.destinationLoc.station_id = std::to_string((int) loc->value.toNumber());
                                        } else if (loc->value.getTag() == JSON_STRING) {
                                            onboardDemand.destinationLoc.station_id = loc->value.toString();
                                        }
                                    } else if (strcmp(loc->key, "direction") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            onboardDemand.destinationLoc.direction = (int) loc->value.toNumber();
                                        }
                                    }
                                }
                            }
                        } else if (strcmp(onboardItem->key, "eta_to_destination") == 0) {
                            if (onboardItem->value.getTag() == JSON_ARRAY && onboardItem->value.toNode()) {
                                int i = 0;
                                for (auto eta : onboardItem->value) {
                                    onboardDemand.etaToDestination[i++] = (int) eta->value.toNumber();
                                    if (i >= 2) {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    request.onboardDemands.push_back(onboardDemand);
                }
            }
        } else if (strcmp(item->key, "onboard_waiting_demands") == 0) {
            if (item->value.getTag() == JSON_ARRAY && item->value.toNode()) {
                for (auto waiting : item->value) {
                    if (waiting->value.getTag() != JSON_OBJECT || !waiting->value.toNode()) {
                        throw std::runtime_error("Invalid JSON request format");
                    }
                    OnboardWaitingDemand waitingDemand;
                    waitingDemand.startLoc.waypoint_id = -1;
                    waitingDemand.startLoc.direction = -1;
                    waitingDemand.destinationLoc.waypoint_id = -1;
                    waitingDemand.destinationLoc.direction = -1;
                    waitingDemand.demand = 1;
                    for (auto waitingItem : waiting->value) {
                        if (strcmp(waitingItem->key, "id") == 0) {
                            if (waitingItem->value.getTag() == JSON_NUMBER) {
                                waitingDemand.id = std::to_string((int) waitingItem->value.toNumber());
                            } else if (waitingItem->value.getTag() == JSON_STRING) {
                                waitingDemand.id = waitingItem->value.toString();
                            }
                        } else if (strcmp(waitingItem->key, "supply_idx") == 0) {
                            if (waitingItem->value.getTag() == JSON_NUMBER) {
                                waitingDemand.supplyIdx = std::to_string((int) waitingItem->value.toNumber());
                            } else if (waitingItem->value.getTag() == JSON_STRING) {
                                waitingDemand.supplyIdx = waitingItem->value.toString();
                            }
                        } else if (strcmp(waitingItem->key, "demand") == 0) {
                            waitingDemand.demand = (int) waitingItem->value.toNumber();
                        } else if (strcmp(waitingItem->key, "start_loc") == 0) {
                            if (waitingItem->value.getTag() == JSON_OBJECT && waitingItem->value.toNode()) {
                                for (auto loc : waitingItem->value) {
                                    if (strcmp(loc->key, "lat") == 0) {
                                        waitingDemand.startLoc.lat = loc->value.toNumber();
                                    } else if (strcmp(loc->key, "lng") == 0) {
                                        waitingDemand.startLoc.lng = loc->value.toNumber();
                                    } else if (strcmp(loc->key, "waypoint_id") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            waitingDemand.startLoc.waypoint_id = (int) loc->value.toNumber();
                                        }
                                    } else if (strcmp(loc->key, "station_id") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            waitingDemand.startLoc.station_id = std::to_string((int) loc->value.toNumber());
                                        } else if (loc->value.getTag() == JSON_STRING) {
                                            waitingDemand.startLoc.station_id = loc->value.toString();
                                        }
                                    } else if (strcmp(loc->key, "direction") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            waitingDemand.startLoc.direction = (int) loc->value.toNumber();
                                        }
                                    }
                                }
                            }
                        } else if (strcmp(waitingItem->key, "destination_loc") == 0) {
                            if (waitingItem->value.getTag() == JSON_OBJECT && waitingItem->value.toNode()) {
                                for (auto loc : waitingItem->value) {
                                    if (strcmp(loc->key, "lat") == 0) {
                                        waitingDemand.destinationLoc.lat = loc->value.toNumber();
                                    } else if (strcmp(loc->key, "lng") == 0) {
                                        waitingDemand.destinationLoc.lng = loc->value.toNumber();
                                    } else if (strcmp(loc->key, "waypoint_id") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            waitingDemand.destinationLoc.waypoint_id = (int) loc->value.toNumber();
                                        }
                                    } else if (strcmp(loc->key, "station_id") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            waitingDemand.destinationLoc.station_id = std::to_string((int) loc->value.toNumber());
                                        } else if (loc->value.getTag() == JSON_STRING) {
                                            waitingDemand.destinationLoc.station_id = loc->value.toString();
                                        }
                                    } else if (strcmp(loc->key, "direction") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            waitingDemand.destinationLoc.direction = (int) loc->value.toNumber();
                                        }
                                    }
                                }
                            }
                        } else if (strcmp(waitingItem->key, "eta_to_start") == 0) {
                            if (waitingItem->value.getTag() == JSON_ARRAY && waitingItem->value.toNode()) {
                                int i = 0;
                                for (auto eta : waitingItem->value) {
                                    waitingDemand.etaToStart[i++] = (int) eta->value.toNumber();
                                    if (i >= 2) {
                                        break;
                                    }
                                }
                            }
                        } else if (strcmp(waitingItem->key, "eta_to_destination") == 0) {
                            if (waitingItem->value.getTag() == JSON_ARRAY && waitingItem->value.toNode()) {
                                int i = 0;
                                for (auto eta : waitingItem->value) {
                                    waitingDemand.etaToDestination[i++] = (int) eta->value.toNumber();
                                    if (i >= 2) {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    request.onboardWaitingDemands.push_back(waitingDemand);
                }
            }
        } else if (strcmp(item->key, "newDemand") == 0 || strcmp(item->key, "new_demands") == 0) {
            if (item->value.getTag() == JSON_ARRAY && item->value.toNode()) {
                for (auto demand : item->value) {
                    if (demand->value.getTag() != JSON_OBJECT && !demand->value.toNode()) {
                        throw std::runtime_error("Invalid JSON request format");
                    }
                    NewDemand newDemand;
                    newDemand.startLoc.waypoint_id = -1;
                    newDemand.startLoc.direction = -1;
                    newDemand.destinationLoc.waypoint_id = -1;
                    newDemand.destinationLoc.direction = -1;
                    newDemand.demand = 1;
                    for (auto demandItem : demand->value) {
                        if (strcmp(demandItem->key, "id") == 0) {
                            if (demandItem->value.getTag() == JSON_NUMBER) {
                                newDemand.id = std::to_string((int) demandItem->value.toNumber());
                            } else if (demandItem->value.getTag() == JSON_STRING) {
                                newDemand.id = demandItem->value.toString();
                            }
                        } else if (strcmp(demandItem->key, "demand") == 0) {
                            newDemand.demand = (int) demandItem->value.toNumber();
                        } else if (strcmp(demandItem->key, "start_loc") == 0) {
                            if (demandItem->value.getTag() == JSON_OBJECT && demandItem->value.toNode()) {
                                for (auto loc : demandItem->value) {
                                    if (strcmp(loc->key, "lat") == 0) {
                                        newDemand.startLoc.lat = loc->value.toNumber();
                                    } else if (strcmp(loc->key, "lng") == 0) {
                                        newDemand.startLoc.lng = loc->value.toNumber();
                                    } else if (strcmp(loc->key, "station_id") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            newDemand.startLoc.station_id = std::to_string((int) loc->value.toNumber());
                                        } else if (loc->value.getTag() == JSON_STRING) {
                                            newDemand.startLoc.station_id = loc->value.toString();
                                        }
                                    } else if (strcmp(loc->key, "direction") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            newDemand.startLoc.direction = (int) loc->value.toNumber();
                                        }
                                    }
                                }
                            }
                        } else if (strcmp(demandItem->key, "destination_loc") == 0) {
                            if (demandItem->value.getTag() == JSON_OBJECT && demandItem->value.toNode()) {
                                for (auto loc : demandItem->value) {
                                    if (strcmp(loc->key, "lat") == 0) {
                                        newDemand.destinationLoc.lat = loc->value.toNumber();
                                    } else if (strcmp(loc->key, "lng") == 0) {
                                        newDemand.destinationLoc.lng = loc->value.toNumber();
                                    } else if (strcmp(loc->key, "station_id") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            newDemand.destinationLoc.station_id = std::to_string((int) loc->value.toNumber());
                                        } else if (loc->value.getTag() == JSON_STRING) {
                                            newDemand.destinationLoc.station_id = loc->value.toString();
                                        }
                                    } else if (strcmp(loc->key, "direction") == 0) {
                                        if (loc->value.getTag() == JSON_NUMBER) {
                                            newDemand.destinationLoc.direction = (int) loc->value.toNumber();
                                        }
                                    }
                                }
                            }
                        } else if (strcmp(demandItem->key, "eta_to_start") == 0) {
                            if (demandItem->value.getTag() == JSON_ARRAY && demandItem->value.toNode()) {
                                int i = 0;
                                for (auto eta : demandItem->value) {
                                    newDemand.etaToStart[i++] = (int) eta->value.toNumber();
                                    if (i >= 2) {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    request.newDemands.push_back(newDemand);
                }
            }
        } else if (strcmp(item->key, "assigned") == 0) {
            if (item->value.getTag() == JSON_ARRAY && item->value.toNode()) {
                for (auto assigned : item->value) {
                    if (assigned->value.getTag() != JSON_OBJECT && !assigned->value.toNode()) {
                        throw std::runtime_error("Invalid JSON request format");
                    }
                    VehicleAssigned vehicleAssigned;
                    for (auto assignedItem : assigned->value) {
                        if (strcmp(assignedItem->key, "supply_idx") == 0) {
                            if (assignedItem->value.getTag() == JSON_NUMBER) {
                                vehicleAssigned.supplyIdx = std::to_string((int) assignedItem->value.toNumber());
                            } else if (assignedItem->value.getTag() == JSON_STRING) {
                                vehicleAssigned.supplyIdx = assignedItem->value.toString();
                            }
                        } else if (strcmp(assignedItem->key, "route_order") == 0) {
                            if (assignedItem->value.getTag() == JSON_ARRAY && assignedItem->value.toNode()) {
                                for (auto routeOrder : assignedItem->value) {
                                    if (routeOrder->value.getTag() != JSON_ARRAY && routeOrder->value.toNode()) {
                                        throw std::runtime_error("Invalid JSON request format");
                                    }
                                    int i = 0;
                                    std::pair<std::string,int> routeOrderPair;
                                    for (auto routeOrderItem : routeOrder->value) {
                                        if (i == 0) {
                                            if (routeOrderItem->value.getTag() == JSON_STRING) {
                                                routeOrderPair.first = routeOrderItem->value.toString();
                                            } else if (routeOrderItem->value.getTag() == JSON_NUMBER) {
                                                routeOrderPair.first = std::to_string((int) routeOrderItem->value.toNumber());
                                            }
                                        } else if (i == 1) routeOrderPair.second = (int) routeOrderItem->value.toNumber();
                                        else break;
                                        i++;
                                    }
                                    vehicleAssigned.routeOrder.push_back(routeOrderPair);
                                }
                            }
                        } else if (strcmp(assignedItem->key, "route_times") == 0) {
                            if (assignedItem->value.getTag() == JSON_ARRAY && assignedItem->value.toNode()) {
                                for (auto routeTime : assignedItem->value) {
                                    vehicleAssigned.routeTimes.push_back((int64_t) routeTime->value.toNumber());
                                }
                            }
                        } else if (strcmp(assignedItem->key, "route_distances") == 0) {
                            if (assignedItem->value.getTag() == JSON_ARRAY && assignedItem->value.toNode()) {
                                for (auto routeTime : assignedItem->value) {
                                    vehicleAssigned.routeDistances.push_back((int64_t) routeTime->value.toNumber());
                                }
                            }
                        }
                    }
                    request.assigned.push_back(vehicleAssigned);
                }
            }
        } else if (strcmp(item->key, "optimize_type") == 0) {
            if (item->value.getTag() == JSON_NUMBER) {
                request.optimizeType = static_cast<OptimizeType>(item->value.toNumber());
            } else if (item->value.getTag() == JSON_STRING) {
                if (strcmp(item->value.toString(), "Time") == 0) {
                    request.optimizeType = OptimizeType::Time;
                } else if (strcmp(item->value.toString(), "Distance") == 0) {
                    request.optimizeType = OptimizeType::Distance;
                } else if (strcmp(item->value.toString(), "Co2") == 0) {
                    request.optimizeType = OptimizeType::Co2;
                } else {
                    throw std::runtime_error("Invalid optimize type");
                }
            }
        } else if (strcmp(item->key, "max_solution_number") == 0) {
            if (item->value.getTag() == JSON_NUMBER) {
                request.maxSolutions = (int) item->value.toNumber();
            }
        } else if (strcmp(item->key, "loc_hash") == 0) {
            if (item->value.getTag() == JSON_STRING) {
                request.locHash = item->value.toString();
            }
        } else if (strcmp(item->key, "date_time") == 0) {
            if (item->value.getTag() == JSON_STRING) {
                std::string dateTime = item->value.toString();
                if (!dateTime.empty()) {
                    request.dateTime = dateTime;
                }
            }
        } else if (strcmp(item->key, "max_duration") == 0) {
            if (item->value.getTag() == JSON_NUMBER) {
                request.maxDuration = (int) item->value.toNumber();
            }
        }
    }

    return request;
}

std::string makeDispatchResponse(
    const ModRequest& modRequest,
    size_t vehicleCount,
    std::unordered_map<int, ModRoute>& mapModRoute,
    const Solution* solution)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8) << "{\"vehicle_routes\": [";
    for (int i = 0; i < solution->n_routes; i++) {
        auto& route = solution->routes[i];
        auto& vehicle = modRequest.vehicleLocs[solution->vehicles[i]];
        oss << (i == 0 ? "" : ",") << "{\"supply_idx\":\"" << vehicle.supplyIdx << "\",\"routes\":[";
        for (int j = 0, add_count = 0; j < route.length; j++) {
            auto& node = route.path[j];
            if (node < 0) {
                continue;
            }
            if (node == 0) {
                continue;
            }
            if (node <= vehicleCount) {
                if (solution->vehicles[i] + 1 != node) {
                    throw std::runtime_error("Invalid vehicle assignment");
                }
            } else {
                oss << (add_count > 0 ? "," : "");
                oss <<  "{\"id\":\"" << mapModRoute[node].id << "\",\"demand\":" << mapModRoute[node].demand;
                oss << ",\"loc\":{\"lat\":" << mapModRoute[node].location.lat << ",\"lng\":" << mapModRoute[node].location.lng;
                if (mapModRoute[node].location.waypoint_id >= 0) {
                    oss << ",\"waypoint_id\":" << mapModRoute[node].location.waypoint_id;
                }
                if (!mapModRoute[node].location.station_id.empty()) {
                    oss << ",\"station_id\":\"" << mapModRoute[node].location.station_id << "\"";
                }
                oss << "}";
#ifdef SOLUTION_INCLUDE_TIME
                auto& arrival_time = route.times[j];
                oss << ",\"arrival_time\":" << arrival_time;
#endif
                oss << "}";
                add_count++;
            }
        }
        oss << "]}";
    }
    oss << "],\"missing\":[";
    if (solution->n_missing > 0) {
        for (int i = 0; i < solution->n_missing; i++) {
            auto& node = solution->missing[i];
            oss << (i > 0 ? "," : "") << "{\"id\":\"" << mapModRoute[node].id << "\",\"demand\":" << mapModRoute[node].demand << "}";
        }
    }
    oss << "],\"unacceptables\":[";
    if (solution->n_unacceptables > 0) {
        for (int i = 0; i < solution->n_unacceptables; i++) {
            auto& node = solution->unacceptables[i];
            oss << (i > 0 ? "," : "") << "{\"id\":\"" << mapModRoute[node].id << "\",\"demand\":" << mapModRoute[node].demand << "}";
        }
    }
    oss << "],\"total_distance\":" << solution->distance << ",\"total_time\":" << solution->time << "}"; 
    return oss.str();
}
