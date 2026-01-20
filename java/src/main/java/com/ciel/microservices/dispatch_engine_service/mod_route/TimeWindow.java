package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.util.Objects;

public class TimeWindow {
    private int low;
    private int high;

    public TimeWindow(int low, int high) {
        this.low = low;
        this.high = high;
    }

    public TimeWindow() {
    }

    public int getLow() {
        return low;
    }

    public void setLow(int low) {
        this.low = low;
    }

    public int getHigh() {
        return high;
    }

    public void setHigh(int high) {
        this.high = high;
    }

    @Override
    public String toString() {
        return "TimeWindow{" +
                "low=" + low +
                ", high=" + high +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (o == null || getClass() != o.getClass()) return false;
        TimeWindow that = (TimeWindow) o;
        return low == that.low && high == that.high;
    }

    @Override
    public int hashCode() {
        return Objects.hash(low, high);
    }
}
