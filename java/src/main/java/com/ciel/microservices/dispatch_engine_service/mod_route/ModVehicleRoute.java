package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.List;
import java.util.Objects;

public class ModVehicleRoute {
    private final String supplyIdx;
    private final List<ModRoute> routes;

    public ModVehicleRoute(String supplyIdx, List<ModRoute> routes) {
        this.supplyIdx = supplyIdx;
        this.routes = routes;
    }

    public String getSupplyIdx() {
        return supplyIdx;
    }

    public List<ModRoute> getRoutes() {
        return routes == null ? List.of() : routes;
    }

    @Override
    public String toString() {
        return "ModVehicleRoute{" +
                "supplyIdx=" + supplyIdx +
                ", routes=" + routes +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (o == null || getClass() != o.getClass()) return false;
        ModVehicleRoute that = (ModVehicleRoute) o;
        return supplyIdx.equals(that.supplyIdx) && Objects.equals(routes, that.routes);
    }

    @Override
    public int hashCode() {
        return Objects.hash(supplyIdx, routes);
    }
}