package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.Objects;

public class DemandAction {
    private final String id;
    private final int demand;

    public DemandAction(String id, int demand) {
        this.id = id;
        this.demand = demand;
    }

    public String getId() {
        return id;
    }

    public int getDemand() {
        return demand;
    }

    @Override
    public String toString() {
        return "DemandAction{" +
                "id=" + id +
                ", demand=" + demand +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (o == null || getClass() != o.getClass()) return false;
        DemandAction that = (DemandAction) o;
        return id.equals(that.id) && demand == that.demand;
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, demand);
    }
}