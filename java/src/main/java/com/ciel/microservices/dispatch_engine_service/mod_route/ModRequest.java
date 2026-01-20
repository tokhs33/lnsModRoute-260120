package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.List;

public class ModRequest {
    private List<VehicleLocation> vehicleLocs;
    private List<OnboardDemand> onboardDemands;
    private List<OnboardWaitingDemand> onboardWaitingDemands;
    private List<NewDemand> newDemands;
    private List<VehicleAssigned> assigned;
    private OptimizeType optimizeType;
    private int maxSolutions;
    private String locHash;
    private String dateTime;

    public List<VehicleLocation> getVehicleLocs() {
        return vehicleLocs == null ? List.of() : vehicleLocs;
    }

    public void setVehicleLocs(List<VehicleLocation> vehicleLocs) {
        this.vehicleLocs = vehicleLocs;
    }

    public List<OnboardDemand> getOnboardDemands() {
        return onboardDemands == null ? List.of() : onboardDemands;
    }

    public void setOnboardDemands(List<OnboardDemand> onboardDemands) {
        this.onboardDemands = onboardDemands;
    }

    public List<OnboardWaitingDemand> getOnboardWaitingDemands() {
        return onboardWaitingDemands == null ? List.of() : onboardWaitingDemands;
    }

    public void setOnboardWaitingDemands(List<OnboardWaitingDemand> onboardWaitingDemands) {
        this.onboardWaitingDemands = onboardWaitingDemands;
    }

    public List<NewDemand> getNewDemands() {
        return newDemands == null ? List.of() : newDemands;
    }

    public void setNewDemands(List<NewDemand> newDemands) {
        this.newDemands = newDemands;
    }

    public List<VehicleAssigned> getAssigned() {
        return assigned == null ? List.of() : assigned;
    }

    public void setAssigned(List<VehicleAssigned> assigned) {
        this.assigned = assigned;
    }

    public OptimizeType getOptimizeType() {
        return optimizeType;
    }

    public void setOptimizeType(OptimizeType optimizeType) {
        this.optimizeType = optimizeType;
    }

    public int getMaxSolutions() {
        return maxSolutions;
    }

    public void setMaxSolutions(int maxSolutions) {
        this.maxSolutions = maxSolutions;
    }

    public String getLocHash() {
        return locHash;
    }

    public void setLocHash(String locHash) {
        this.locHash = locHash;
    }

    public String getDateTime() {
        return dateTime;
    }

    public void setDateTime(String dateTime) {
        this.dateTime = dateTime;
    }
}