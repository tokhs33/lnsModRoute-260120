package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.Objects;

public class OnboardDemand {
    private String id;
    private int demand;
    private String supplyIdx;
    private Location destinationLoc;
    private TimeWindow etaToDestination;

    public String getId() {
        return id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public int getDemand() {
        return demand;
    }

    public void setDemand(int demand) {
        this.demand = demand;
    }

    public String getSupplyIdx() {
        return supplyIdx;
    }

    public void setSupplyIdx(String supplyIdx) {
        this.supplyIdx = supplyIdx;
    }

    public Location getDestinationLoc() {
        return destinationLoc;
    }

    public void setDestinationLoc(Location destinationLoc) {
        this.destinationLoc = destinationLoc;
    }

    public TimeWindow getEtaToDestination() {
        return etaToDestination;
    }

    public void setEtaToDestination(TimeWindow etaToDestination) {
        this.etaToDestination = etaToDestination;
    }

    @Override
    public String toString() {
        return "OnboardDemand{" +
                "id=" + id +
                ", demand=" + demand +
                ", supplyIdx=" + supplyIdx +
                ", destinationLoc=" + destinationLoc +
                ", etaToDestination=" + etaToDestination +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (o == null || getClass() != o.getClass()) return false;
        OnboardDemand that = (OnboardDemand) o;
        return id.equals(that.id) && demand == that.demand && supplyIdx.equals(that.supplyIdx) && Objects.equals(destinationLoc, that.destinationLoc) && Objects.equals(etaToDestination, that.etaToDestination);
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, demand, supplyIdx, destinationLoc, etaToDestination);
    }
}