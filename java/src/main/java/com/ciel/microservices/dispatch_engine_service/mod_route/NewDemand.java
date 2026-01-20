package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.Objects;

public class NewDemand {
    private String id;
    private int demand;
    private Location startLoc;
    private Location destinationLoc;
    private TimeWindow etaToStart;

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

    @Override
    public String toString() {
        return "NewDemand{" +
                "id=" + id +
                ", demand=" + demand +
                ", startLoc=" + startLoc +
                ", destinationLoc=" + destinationLoc +
                ", etaToStart=" + etaToStart +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (o == null || getClass() != o.getClass()) return false;
        NewDemand newDemand = (NewDemand) o;
        return id.equals(newDemand.id) && demand == newDemand.demand && Objects.equals(startLoc, newDemand.startLoc) && Objects.equals(destinationLoc, newDemand.destinationLoc) && Objects.equals(etaToStart, newDemand.etaToStart);
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, demand, startLoc, destinationLoc, etaToStart);
    }
}