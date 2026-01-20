package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.Objects;

public class VehicleLocation {
    private String supplyIdx;
    private int capacity;
    private Location location;

    public String getSupplyIdx() {
        return supplyIdx;
    }

    public void setSupplyIdx(String supplyIdx) {
        this.supplyIdx = supplyIdx;
    }

    public int getCapacity() {
        return capacity;
    }

    public void setCapacity(int capacity) {
        this.capacity = capacity;
    }

    public Location getLocation() {
        return location;
    }

    public void setLocation(Location location) {
        this.location = location;
    }

    @Override
    public String toString() {
        return "VehicleLocation{" +
                "supplyIdx=" + supplyIdx +
                ", capacity=" + capacity +
                ", location=" + location +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (o == null || getClass() != o.getClass()) return false;
        VehicleLocation that = (VehicleLocation) o;
        return supplyIdx.equals(that.supplyIdx) && capacity == that.capacity && Objects.equals(location, that.location);
    }

    @Override
    public int hashCode() {
        return Objects.hash(supplyIdx, capacity, location);
    }
}
