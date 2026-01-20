package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.Objects;

public class ModRoute {
    private final String id;
    private final int demand;
    private final Location location;

    public ModRoute(String id, int demand, Location location) {
        this.id = id;
        this.demand = demand;
        this.location = location;
    }

    public String getId() {
        return id;
    }

    public int getDemand() {
        return demand;
    }

    public Location getLocation() {
        return location;
    }

    @Override
    public String toString() {
        return "ModRoute{" +
                "id=" + id +
                ", demand=" + demand +
                ", location=" + location +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (o == null || getClass() != o.getClass()) return false;
        ModRoute modRoute = (ModRoute) o;
        return id.equals(modRoute.id) && demand == modRoute.demand && Objects.equals(location, modRoute.location);
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, demand, location);
    }
}