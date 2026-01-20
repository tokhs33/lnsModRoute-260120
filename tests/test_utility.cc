#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <gason/gason.h>
#include "test_utility.h"

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
                            vehicleLoc.supplyIdx = (int) vehicleItem->value.toNumber();
                        } else if (strcmp(vehicleItem->key, "capacity") == 0) {
                            vehicleLoc.capacity = (int) vehicleItem->value.toNumber();
                        } else if (strcmp(vehicleItem->key, "lat") == 0) {
                            vehicleLoc.location.lat = vehicleItem->value.toNumber();
                        } else if (strcmp(vehicleItem->key, "lng") == 0) {
                            vehicleLoc.location.lng = vehicleItem->value.toNumber();
                        } else if (strcmp(vehicleItem->key, "direction") == 0) {
                            vehicleLoc.location.direction = (int) vehicleItem->value.toNumber();
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
                    onboardDemand.destinationLoc.station_id = -1;
                    onboardDemand.destinationLoc.direction = -1;
                    onboardDemand.demand = 1;
                    for (auto onboardItem : onboard->value) {
                        if (strcmp(onboardItem->key, "id") == 0) {
                            onboardDemand.id = (int) onboardItem->value.toNumber();
                        } else if (strcmp(onboardItem->key, "supply_idx") == 0) {
                            onboardDemand.supplyIdx = (int) onboardItem->value.toNumber();
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
                                        onboardDemand.destinationLoc.waypoint_id = (int) loc->value.toNumber();
                                    } else if (strcmp(loc->key, "station_id") == 0) {
                                        onboardDemand.destinationLoc.station_id = (int) loc->value.toNumber();
                                    } else if (strcmp(loc->key, "direction") == 0) {
                                        onboardDemand.destinationLoc.direction = (int) loc->value.toNumber();
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
                    waitingDemand.startLoc.station_id = -1;
                    waitingDemand.startLoc.direction = -1;
                    waitingDemand.destinationLoc.waypoint_id = -1;
                    waitingDemand.destinationLoc.station_id = -1;
                    waitingDemand.destinationLoc.direction = -1;
                    waitingDemand.demand = 1;
                    for (auto waitingItem : waiting->value) {
                        if (strcmp(waitingItem->key, "id") == 0) {
                            waitingDemand.id = (int) waitingItem->value.toNumber();
                        } else if (strcmp(waitingItem->key, "supply_idx") == 0) {
                            waitingDemand.supplyIdx = (int) waitingItem->value.toNumber();
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
                                        waitingDemand.startLoc.waypoint_id = (int) loc->value.toNumber();
                                    } else if (strcmp(loc->key, "station_id") == 0) {
                                        waitingDemand.startLoc.station_id = (int) loc->value.toNumber();
                                    } else if (strcmp(loc->key, "direction") == 0) {
                                        waitingDemand.startLoc.direction = (int) loc->value.toNumber();
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
                                        waitingDemand.destinationLoc.waypoint_id = (int) loc->value.toNumber();
                                    } else if (strcmp(loc->key, "station_id") == 0) {
                                        waitingDemand.destinationLoc.station_id = (int) loc->value.toNumber();
                                    } else if (strcmp(loc->key, "direction") == 0) {
                                        waitingDemand.destinationLoc.direction = (int) loc->value.toNumber();
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
                    newDemand.startLoc.station_id = -1;
                    newDemand.startLoc.direction = -1;
                    newDemand.destinationLoc.waypoint_id = -1;
                    newDemand.destinationLoc.station_id = -1;
                    newDemand.destinationLoc.direction = -1;
                    newDemand.demand = 1;
                    for (auto demandItem : demand->value) {
                        if (strcmp(demandItem->key, "id") == 0) {
                            newDemand.id = (int) demandItem->value.toNumber();
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
                                        newDemand.startLoc.station_id = (int) loc->value.toNumber();
                                    } else if (strcmp(loc->key, "direction") == 0) {
                                        newDemand.startLoc.direction = (int) loc->value.toNumber();
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
                                        newDemand.destinationLoc.station_id = (int) loc->value.toNumber();
                                    } else if (strcmp(loc->key, "direction") == 0) {
                                        newDemand.destinationLoc.direction = (int) loc->value.toNumber();
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
                            vehicleAssigned.supplyIdx = (int) assignedItem->value.toNumber();
                        } else if (strcmp(assignedItem->key, "route_order") == 0) {
                            if (assignedItem->value.getTag() == JSON_ARRAY && assignedItem->value.toNode()) {
                                for (auto routeOrder : assignedItem->value) {
                                    if (routeOrder->value.getTag() != JSON_ARRAY && routeOrder->value.toNode()) {
                                        throw std::runtime_error("Invalid JSON request format");
                                    }
                                    int i = 0;
                                    std::pair<int,int> routeOrderPair;
                                    for (auto routeOrderItem : routeOrder->value) {
                                        if (i == 0) routeOrderPair.first = (int) routeOrderItem->value.toNumber();
                                        else if (i == 1) routeOrderPair.second = (int) routeOrderItem->value.toNumber();
                                        else break;
                                        i++;
                                    }
                                    vehicleAssigned.routeOrder.push_back(routeOrderPair);
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
            request.maxSolutions = (int) item->value.toNumber();
        } else if (strcmp(item->key, "loc_hash") == 0) {
            request.locHash = item->value.toString();
        } else if (strcmp(item->key, "date_time") == 0) {
            std::string dateTime = item->value.toString();
            if (!dateTime.empty()) {
                request.dateTime = dateTime;
            }
        }
    }

    return request;
}

std::string readFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Fail to open file");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string logNow()
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm *now_tm = std::localtime(&now_c);
    std::ostringstream oss;
    oss << "[" << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S") << "]";
    return oss.str();
}

void pushStationToIdx(StationToIdxMap& stationToIdx, const std::string& stationId, int direction, int idx)
{
    if (stationId.empty()) {
        // stationId < 0 은 station 정보가 없다는 것이므로, 캐싱이 불가능 (하려면 위치 정보를 이용해야 되는데, 아직은 처리 안함)
        // 캐싱을 할 수 없어서 stationToIdx 에 값을 무조건 넣어야 하는 방식으로 -idx 값을 key로 넣어서 처리
        // 그러면 station이 있는 경우나 없는 경우 모두 stationToIdx에 값이 들어가게 됨
        stationToIdx[std::make_pair(std::to_string(-idx), -1)] = idx;
    } else {
        auto it = stationToIdx.find(std::make_pair(stationId, direction));
        if (it == stationToIdx.end()) {
            stationToIdx[std::make_pair(stationId, direction)] = idx;
        }
    }
}

std::string parseModIdx(int idx, const ModRequest& request)
{
    if (idx < request.vehicleLocs.size()) {
        return "vehicle " + std::to_string(idx);
    }
    idx -= request.vehicleLocs.size();
    if (idx < request.onboardDemands.size()) {
        return "onboard " + std::to_string(idx);
    }
    idx -= request.onboardDemands.size();
    if (idx < request.onboardWaitingDemands.size() * 2) {
        return "waiting " + std::to_string(idx / 2) + (idx % 2 == 0 ? " start" : " dest");
    }
    idx -= request.onboardWaitingDemands.size() * 2;
    if (idx < request.newDemands.size() * 2) {
        return "new " + std::to_string(idx / 2) + (idx % 2 == 0 ? " start" : " dest");
    }
    return "unknown";
}

std::string parseCacheIdx(int idx, size_t nodeCount, const ModRequest& request)
{
    std::ostringstream oss;
    int to_idx = idx % (nodeCount + 1);
    int from_idx = (idx - to_idx) / (nodeCount + 1);
    oss << "(" << parseModIdx(from_idx - 1, request) << "), (" << parseModIdx(to_idx - 1, request) << ")";
    return oss.str();
}

