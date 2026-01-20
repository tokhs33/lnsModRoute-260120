package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.List;
import java.util.Objects;

public class VehicleAssigned {
    private final String supplyIdx;
    private final List<DemandAction> routeOrder;
    private final List<Long> routeTimes;
    private final List<Long> routeDistances;

    public VehicleAssigned(String supplyIdx, List<DemandAction> routeOrder, List<Long> routeTimes, List<Long> routeDistances) {
        this.supplyIdx = supplyIdx;
        this.routeOrder = routeOrder;
        this.routeTimes = routeTimes;
        this.routeDistances = routeDistances;
    }

    public String getSupplyIdx() {
        return supplyIdx;
    }

    public List<DemandAction> getRouteOrder() {
        return routeOrder == null ? List.of() : routeOrder;
    }

    public List<Long> getRouteTimes() {
        return routeTimes == null ? List.of() : routeTimes;
    }

    public List<Long> getRouteDistances() {
        return routeDistances == null ? List.of() : routeDistances;
    }

    @Override
    public String toString() {
        return "VehicleAssigned{" +
                "supplyIdx=" + supplyIdx +
                ", routeOrder=" + routeOrder +
                ", routeTimes=" + routeTimes +
                ", routeDistances=" + routeDistances +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (o == null || getClass() != o.getClass()) return false;
        VehicleAssigned that = (VehicleAssigned) o;
        return supplyIdx.equals(that.supplyIdx) && Objects.equals(routeOrder, that.routeOrder) &&
                Objects.equals(routeTimes, that.routeTimes) && Objects.equals(routeDistances, that.routeDistances);
    }

    @Override
    public int hashCode() {
        return Objects.hash(supplyIdx, routeOrder, routeTimes, routeDistances);
    }
}