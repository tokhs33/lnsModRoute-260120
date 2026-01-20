package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.Objects;

public class OnboardWaitingDemand {
    private String id;
    private int demand;
    private String supplyIdx;
    private Location startLoc;
    private Location destinationLoc;
    private TimeWindow etaToStart;
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

    public Location getStartLoc() {
        return startLoc;
    }

    public void setStartLoc(Location startLoc) {
        this.startLoc = startLoc;
    }

    public Location getDestinationLoc() {
        return destinationLoc;
    }

    public void setDestinationLoc(Location destinationLoc) {
        this.destinationLoc = destinationLoc;
    }

    public TimeWindow getEtaToStart() {
        return etaToStart;
    }

    public void setEtaToStart(TimeWindow etaToStart) {
        this.etaToStart = etaToStart;
    }

    public TimeWindow getEtaToDestination() {
        return etaToDestination;
    }

    public void setEtaToDestination(TimeWindow etaToDestination) {
        this.etaToDestination = etaToDestination;
    }

    @Override
    public String toString() {
        return "OnboardWaitingDemand{" +
                "id=" + id +
                ", demand=" + demand +
                ", supplyIdx=" + supplyIdx +
                ", startLoc=" + startLoc +
                ", destinationLoc=" + destinationLoc +
                ", etaToStart=" + etaToStart +
                ", etaToDestination=" + etaToDestination +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (o == null || getClass() != o.getClass()) return false;
        OnboardWaitingDemand that = (OnboardWaitingDemand) o;
        return id.equals(that.id) && demand == that.demand && supplyIdx.equals(that.supplyIdx) && Objects.equals(startLoc, that.startLoc) && Objects.equals(destinationLoc, that.destinationLoc) && Objects.equals(etaToStart, that.etaToStart) && Objects.equals(etaToDestination, that.etaToDestination);
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, demand, supplyIdx, startLoc, destinationLoc, etaToStart, etaToDestination);
    }
}