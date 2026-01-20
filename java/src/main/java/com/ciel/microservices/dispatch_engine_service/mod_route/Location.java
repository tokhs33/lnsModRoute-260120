package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.Objects;

public class Location {
    private double longitude;
    private double latitude;
    private int waypointId;
    private String stationId;
    private int direction;

    public Location() {
        longitude = 0;
        latitude = 0;
        waypointId = -1;
        direction = -1;
    }

    public Location(double longitude, double latitude) {
        this.longitude = longitude;
        this.latitude = latitude;
        waypointId = -1;
        direction = -1;
    }

    public Location(double longitude, double latitude, int waypointId, String stationId, int direction) {
        this.longitude = longitude;
        this.latitude = latitude;
        this.waypointId = waypointId;
        this.stationId = stationId;
        this.direction = direction;
    }

    public double getLongitude() {
        return longitude;
    }

    public void setLongitude(double longitude) {
        this.longitude = longitude;
    }

    public double getLatitude() {
        return latitude;
    }

    public void setLatitude(double latitude) {
        this.latitude = latitude;
    }

    public int getWaypointId() {
        return waypointId;
    }

    public void setWaypointId(int waypointId) {
        this.waypointId = waypointId;
    }

    public String getStationId() {
        return stationId;
    }

    public void setStationId(String stationId) {
        this.stationId = stationId;
    }

    public int getDirection() {
        return direction;
    }

    public void setDirection(int direction) {
        this.direction = direction;
    }

    @Override
    public String toString() {
        return "Location{" +
                "longitude=" + longitude +
                ", latitude=" + latitude +
                ", waypointId=" + waypointId +
                ", stationId=" + stationId +
                ", direction=" + direction +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (o == null || getClass() != o.getClass()) return false;
        Location location = (Location) o;
        return Double.compare(longitude, location.longitude) == 0 && Double.compare(latitude, location.latitude) == 0 && waypointId == location.waypointId && stationId.equals(location.stationId) && direction == location.direction;
    }

    @Override
    public int hashCode() {
        return Objects.hash(longitude, latitude, waypointId, stationId, direction);
    }
}
