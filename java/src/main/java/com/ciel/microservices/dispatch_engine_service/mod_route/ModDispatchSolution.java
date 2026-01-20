package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.List;
import java.util.Objects;
import java.util.Optional;

public class ModDispatchSolution {
    private List<ModVehicleRoute> vehicleRoutes;
    private List<ModRoute> missing;
    private List<ModRoute> unacceptables;
    private long totalDistance;
    private long totalTime;

    public ModDispatchSolution() {
    }

    public ModDispatchSolution(List<ModVehicleRoute> vehicleRoutes, List<ModRoute> missing, List<ModRoute> unacceptables, long totalDistance, long totalTime) {
        this.vehicleRoutes = vehicleRoutes;
        this.missing = missing;
        this.unacceptables = unacceptables;
        this.totalDistance = totalDistance;
        this.totalTime = totalTime;
    }

    public List<ModVehicleRoute> getVehicleRoutes() {
        return vehicleRoutes == null ? List.of() : vehicleRoutes;
    }

    public Optional<List<ModRoute>> getMissing() {
        return Optional.ofNullable(missing);
    }

    public Optional<List<ModRoute>> getUnacceptables() {
        return Optional.ofNullable(unacceptables);
    }

    public long getTotalDistance() {
        return totalDistance;
    }

    public long getTotalTime() {
        return totalTime;
    }

    @Override
    public String toString() {
        return "ModDispatchSolution{" +
                "vehicleRoutes=" + vehicleRoutes +
                ", missing=" + missing +
                ", unacceptables=" + unacceptables +
                ", totalDistance=" + totalDistance +
                ", totalTime=" + totalTime +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (o == null || getClass() != o.getClass()) return false;
        ModDispatchSolution that = (ModDispatchSolution) o;
        return totalDistance == that.totalDistance && totalTime == that.totalTime && Objects.equals(vehicleRoutes, that.vehicleRoutes) && Objects.equals(missing, that.missing) && Objects.equals(unacceptables, that.unacceptables);
    }

    @Override
    public int hashCode() {
        return Objects.hash(vehicleRoutes, missing, unacceptables, totalDistance, totalTime);
    }
}