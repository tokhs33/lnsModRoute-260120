package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.io.FileReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.Scanner;
import org.json.simple.JSONObject;
import org.json.simple.JSONArray;
import org.json.simple.parser.JSONParser;

public class ModRouteEngineTest {

    public void run(String filename, String cachePath, String routePath, RouteType routeType) {
        ModRouteEngine engine = new ModRouteEngine();
        AlgorithmParameters ap = engine.default_algorithm_parameters();
        ModRouteConfiguration conf = engine.default_mod_route_configuration();

        System.out.println("Hello, World!");
        System.out.println("ap.time_limit = " + ap.time_limit);
        System.out.println("ap.thread_count = " + ap.thread_count);
        System.out.println("conf.maxDuration = " + conf.maxDuration);
        System.out.println("conf.serviceTime = " + conf.serviceTime);

        if (filename == null) {
            return;
        }

        ap.enable_missing_solution = true;

        ModRequest request = readFile(filename);
        var result = engine.runOptimize(request, routePath, routeType, 4, cachePath, ap, conf);
        for (ModDispatchSolution solution : result) {
            System.out.println(solution);
        }
    }

    private Location  parseLocation(JSONObject jsonObject) {
        Location location = new Location((double) jsonObject.get("lng"), (double) jsonObject.get("lat"));
        if (jsonObject.containsKey("waypoint_id")) {
            location.setWaypointId((int) (long) jsonObject.get("waypoint_id"));
        }
        if (jsonObject.containsKey("station_id")) {
            location.setStationId((String) jsonObject.get("station_id"));
        }
        if (jsonObject.containsKey("direction")) {
            location.setDirection((int) (long) jsonObject.get("direction"));
        }
        return location;
    }

    private TimeWindow parseTimeWindow(JSONArray jsonArray) {
        return new TimeWindow((int) (long) jsonArray.get(0), (int) (long) jsonArray.get(1));
    }

    private List<VehicleLocation> parseVehicleLocations(JSONArray jsonArray) {
        List<VehicleLocation> vehicleLocs = new ArrayList<>();
        for (Object obj : jsonArray) {
            JSONObject jsonObject = (JSONObject) obj;
            VehicleLocation vehicleLoc = new VehicleLocation();
            vehicleLoc.setSupplyIdx((String) jsonObject.get("supply_idx"));
            vehicleLoc.setCapacity((int) (long) jsonObject.get("capacity"));
            vehicleLoc.setLocation(parseLocation(jsonObject));
            vehicleLocs.add(vehicleLoc);
        }
        return vehicleLocs;
    }

    private List<OnboardDemand> parseOnboardDemands(JSONArray jsonArray) {
        List<OnboardDemand> onboardDemands = new ArrayList<>();
        for (Object obj : jsonArray) {
            JSONObject jsonObject = (JSONObject) obj;
            OnboardDemand onboardDemand = new OnboardDemand();
            onboardDemand.setId((String) jsonObject.get("id"));
            onboardDemand.setDemand((int) (long) jsonObject.get("demand"));
            onboardDemand.setSupplyIdx((String) jsonObject.get("supply_idx"));
            onboardDemand.setDestinationLoc(parseLocation((JSONObject) jsonObject.get("destination_loc")));
            onboardDemand.setEtaToDestination(parseTimeWindow((JSONArray) jsonObject.get("eta_to_destination")));
            onboardDemands.add(onboardDemand);
        }
        return onboardDemands;
    }

    private List<OnboardWaitingDemand> parseOnboardWaitingDemands(JSONArray jsonArray) {
        List<OnboardWaitingDemand> onboardWaitingDemands = new ArrayList<>();
        for (Object obj : jsonArray) {
            JSONObject jsonObject = (JSONObject) obj;
            OnboardWaitingDemand onboardWaitingDemand = new OnboardWaitingDemand();
            onboardWaitingDemand.setId((String) jsonObject.get("id"));
            onboardWaitingDemand.setDemand((int) (long) jsonObject.get("demand"));
            onboardWaitingDemand.setSupplyIdx((String) jsonObject.get("supply_idx"));
            onboardWaitingDemand.setStartLoc(parseLocation((JSONObject) jsonObject.get("start_loc")));
            onboardWaitingDemand.setDestinationLoc(parseLocation((JSONObject) jsonObject.get("destination_loc")));
            onboardWaitingDemand.setEtaToStart(parseTimeWindow((JSONArray) jsonObject.get("eta_to_start")));
            onboardWaitingDemand.setEtaToDestination(parseTimeWindow((JSONArray) jsonObject.get("eta_to_destination")));
            onboardWaitingDemands.add(onboardWaitingDemand);
        }
        return onboardWaitingDemands;
    }

    private List<NewDemand> parseNewDemands(JSONArray jsonArray) {
        List<NewDemand> newDemands = new ArrayList<>();
        for (Object obj : jsonArray) {
            JSONObject jsonObject = (JSONObject) obj;
            NewDemand newDemand = new NewDemand();
            newDemand.setId((String) jsonObject.get("id"));
            newDemand.setDemand((int) (long) jsonObject.get("demand"));
            newDemand.setStartLoc(parseLocation((JSONObject) jsonObject.get("start_loc")));
            newDemand.setDestinationLoc(parseLocation((JSONObject) jsonObject.get("destination_loc")));
            newDemand.setEtaToStart(parseTimeWindow((JSONArray) jsonObject.get("eta_to_start")));
            newDemands.add(newDemand);
        }
        return newDemands;
    }

    private List<DemandAction> parseRouteOrders(JSONArray jsonArray) {
        List<DemandAction> routeOrders = new ArrayList<>();
        for (Object obj : jsonArray) {
            JSONArray items = (JSONArray) obj;
            DemandAction demandAction = new DemandAction((String) items.get(0), (int) (long) items.get(1));
            routeOrders.add(demandAction);
        }
        return routeOrders;
    }

    private List<Long> parseLongList(JSONArray jsonArray) {
        List<Long> longList = new ArrayList<>();
        for (Object obj : jsonArray) {
            longList.add((long) obj);
        }
        return longList;
    }

    private List<VehicleAssigned> parseVehicleAssigned(JSONArray jsonArray) {
        List<VehicleAssigned> vehicleAssigns = new ArrayList<>();
        for (Object obj : jsonArray) {
            JSONObject jsonObject = (JSONObject) obj;
            VehicleAssigned vehicleAssigned = new VehicleAssigned(
                    (String) jsonObject.get("supply_idx"),
                    parseRouteOrders((JSONArray) jsonObject.get("route_order")),
                    parseLongList((JSONArray) jsonObject.get("route_times")),
                    parseLongList((JSONArray) jsonObject.get("route_distances")));
            vehicleAssigns.add(vehicleAssigned);
        }
        return vehicleAssigns;
    }

    private OptimizeType parseOptimizeType(String value) {
        return OptimizeType.valueOf(value);
    }

    public ModRequest readFile(String filename) {
        JSONParser parser = new JSONParser();
        ModRequest request = new ModRequest();
        try (FileReader fr = new FileReader(filename, StandardCharsets.UTF_8)) {
            Object obj = parser.parse(fr);
            JSONObject jsonObject = (JSONObject) obj;
            request.setVehicleLocs(parseVehicleLocations((JSONArray) jsonObject.get("vehicle_locs")));
            request.setOnboardDemands(parseOnboardDemands((JSONArray) jsonObject.get("onboard_demands")));
            request.setOnboardWaitingDemands(parseOnboardWaitingDemands((JSONArray) jsonObject.get("onboard_waiting_demands")));
            request.setNewDemands(parseNewDemands((JSONArray) jsonObject.get("newDemand")));
            request.setAssigned(parseVehicleAssigned((JSONArray) jsonObject.get("assigned")));
            request.setOptimizeType(parseOptimizeType((String) jsonObject.get("optimize_type")));
            request.setLocHash((String) jsonObject.get("loc_hash"));
        } catch (Exception e) {
            e.printStackTrace();
        }
        return request;
    }

    public static void main(String[] args) {
        System.out.println("ProcessID: " + ProcessHandle.current().pid());
        System.out.println("Current relative path is: " + Paths.get("").toAbsolutePath());

        Scanner input = new Scanner(System.in);
        System.out.println("Press Enter to continue(1)...");
        input.nextLine();

        String filename = args.length > 0 ? args[0] : null;
        String cachePath = args.length > 1 ? args[1] : null;
        String routePath = args.length > 2 ? args[2] : "http://localhost:8002";
        RouteType routeType = args.length > 3 ? RouteType.valueOf(args[3]) : RouteType.Valhalla;

        ModRouteEngineTest test = new ModRouteEngineTest();
        test.run(filename, cachePath, routePath, routeType);

        input.close();
    }
}