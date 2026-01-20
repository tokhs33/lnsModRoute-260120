#include <vector>
#include <unordered_map>
#include <optional>
#include <utility>
#include "jni_modroute.h"
#include "lib_modroute.h"

class JniConverter {
public:
    JniConverter(JNIEnv *env) {
        // java 클래스 경로를 지정하여 찾기
        algorithmParametersClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/AlgorithmParameters");
        // java 생성자 메서드ID 가져오기
        algorithmParameterConstructor = env->GetMethodID(algorithmParametersClass, "<init>", "()V");
        // java 필드 메서드ID 가져오기
        nbIterationsField = env->GetFieldID(algorithmParametersClass, "nb_iterations", "I");
        timeLimitField = env->GetFieldID(algorithmParametersClass, "time_limit", "I");
        threadCountField = env->GetFieldID(algorithmParametersClass, "thread_count", "I");
        shawPhiDistanceField = env->GetFieldID(algorithmParametersClass, "shaw_phi_distance", "D");
        shawChiTimeField = env->GetFieldID(algorithmParametersClass, "shaw_chi_time", "D");
        shawPsiCapacityField = env->GetFieldID(algorithmParametersClass, "shaw_psi_capacity", "D");
        shawRemovalPField = env->GetFieldID(algorithmParametersClass, "shaw_removal_p", "I");
        worstRemovalPField = env->GetFieldID(algorithmParametersClass, "worst_removal_p", "I");
        simulatedAnnealingStartTempControlWField = env->GetFieldID(algorithmParametersClass, "simulated_annealing_start_temp_control_w", "D");
        simulatedAnnealingCoolingRateCField = env->GetFieldID(algorithmParametersClass, "simulated_annealing_cooling_rate_c", "D");
        adaptiveWeightAdjD1Field = env->GetFieldID(algorithmParametersClass, "adaptive_weight_adj_d1", "D");
        adaptiveWeightAdjD2Field = env->GetFieldID(algorithmParametersClass, "adaptive_weight_adj_d2", "D");
        adaptiveWeightAdjD3Field = env->GetFieldID(algorithmParametersClass, "adaptive_weight_adj_d3", "D");
        adaptiveWeightDacayRField = env->GetFieldID(algorithmParametersClass, "adaptive_weight_dacay_r", "D");
        insertionObjectiveNoiseNField = env->GetFieldID(algorithmParametersClass, "insertion_objective_noise_n", "D");
        removalReqIterationControlEField = env->GetFieldID(algorithmParametersClass, "removal_req_iteration_control_e", "D");
        delaytimePenaltyField = env->GetFieldID(algorithmParametersClass, "delaytime_penalty", "D");
        waittimePenaltyField = env->GetFieldID(algorithmParametersClass, "waittime_penalty", "D");
        seedField = env->GetFieldID(algorithmParametersClass, "seed", "I");
        enableMissingSolutionField = env->GetFieldID(algorithmParametersClass, "enable_missing_solution", "Z");
        skipRemoveRouteField = env->GetFieldID(algorithmParametersClass, "skip_remove_route", "Z");
        unfeasibleDelaytimeField = env->GetFieldID(algorithmParametersClass, "unfeasible_delaytime", "I");

        // java class 경로
        modRouteConfigurationClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/ModRouteConfiguration");
        // java 생성자 메서드ID 가져오기
        modRouteConfigurationConstructor = env->GetMethodID(modRouteConfigurationClass, "<init>", "()V");
        // java 필드 메서드ID 가져오기
        maxDurationField = env->GetFieldID(modRouteConfigurationClass, "maxDuration", "I");
        bypassRatioField = env->GetFieldID(modRouteConfigurationClass, "bypassRatio", "I");
        serviceTimeField = env->GetFieldID(modRouteConfigurationClass, "serviceTime", "I");
        acceptableBufferField = env->GetFieldID(modRouteConfigurationClass, "acceptableBuffer", "I");
        logRequestField = env->GetFieldID(modRouteConfigurationClass, "logRequest", "Z");
        cacheExpirationTimeField = env->GetFieldID(modRouteConfigurationClass, "cacheExpirationTime", "I");
        solutionLimitField = env->GetFieldID(modRouteConfigurationClass, "solutionLimit", "I");

        listClass = env->FindClass("java/util/List");
        sizeMethod = env->GetMethodID(listClass, "size", "()I");
        getMethodInList = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

        arrayListClass = env->FindClass("java/util/ArrayList");
        arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "()V");
        addMethod = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

        longClass = env->FindClass("java/lang/Long");
        longValueMethod = env->GetMethodID(longClass, "longValue", "()J");

        modRequestClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/ModRequest");
        getVehicleLocsMethod = env->GetMethodID(modRequestClass, "getVehicleLocs", "()Ljava/util/List;");
        getOnboardDemandsMethod = env->GetMethodID(modRequestClass, "getOnboardDemands", "()Ljava/util/List;");
        getOnboardWaitingDemandsMethod = env->GetMethodID(modRequestClass, "getOnboardWaitingDemands", "()Ljava/util/List;");
        getNewDemandsMethod = env->GetMethodID(modRequestClass, "getNewDemands", "()Ljava/util/List;");
        getAssignedMethod = env->GetMethodID(modRequestClass, "getAssigned", "()Ljava/util/List;");
        getOptimizeTypeMethod = env->GetMethodID(modRequestClass, "getOptimizeType", "()Lcom/ciel/microservices/dispatch_engine_service/mod_route/OptimizeType;");
        getMaxSolutionsMethod = env->GetMethodID(modRequestClass, "getMaxSolutions", "()I");
        getLocHashMethod = env->GetMethodID(modRequestClass, "getLocHash", "()Ljava/lang/String;");
        getDateTimeMethod = env->GetMethodID(modRequestClass, "getDateTime", "()Ljava/lang/String;");

        locationClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/Location");
        locationConstructor = env->GetMethodID(locationClass, "<init>", "(DDILjava/lang/String;I)V");
        getLongitudeMethod = env->GetMethodID(locationClass, "getLongitude", "()D");
        getLatitudeMethod = env->GetMethodID(locationClass, "getLatitude", "()D");
        getWaypointIdMethod = env->GetMethodID(locationClass, "getWaypointId", "()I");
        getStationIdMethod = env->GetMethodID(locationClass, "getStationId", "()Ljava/lang/String;");
        getDirectionMethod = env->GetMethodID(locationClass, "getDirection", "()I");

        vehicleLocationClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/VehicleLocation");
        getSupplyIdxMethod = env->GetMethodID(vehicleLocationClass, "getSupplyIdx", "()Ljava/lang/String;");
        getCapacityMethod = env->GetMethodID(vehicleLocationClass, "getCapacity", "()I");
        getLocationMethod = env->GetMethodID(vehicleLocationClass, "getLocation", "()Lcom/ciel/microservices/dispatch_engine_service/mod_route/Location;");

        timeWindowClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/TimeWindow");
        getLowMethod = env->GetMethodID(timeWindowClass, "getLow", "()I");
        getHighMethod = env->GetMethodID(timeWindowClass, "getHigh", "()I");

        onboardDemandClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/OnboardDemand");
        onboardDemandIdMethod = env->GetMethodID(onboardDemandClass, "getId", "()Ljava/lang/String;");
        onboardDemandDemandMethod = env->GetMethodID(onboardDemandClass, "getDemand", "()I");
        onboardDemandSupplyIdxMethod = env->GetMethodID(onboardDemandClass, "getSupplyIdx", "()Ljava/lang/String;");
        onboardDemandDestinationLocMethod = env->GetMethodID(onboardDemandClass, "getDestinationLoc", "()Lcom/ciel/microservices/dispatch_engine_service/mod_route/Location;");
        onboardDemandEtaToDestinationMethod = env->GetMethodID(onboardDemandClass, "getEtaToDestination", "()Lcom/ciel/microservices/dispatch_engine_service/mod_route/TimeWindow;");

        waitingDemandClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/OnboardWaitingDemand");
        waitingDemandIdMethod = env->GetMethodID(waitingDemandClass, "getId", "()Ljava/lang/String;");
        waitingDemandDemandMethod = env->GetMethodID(waitingDemandClass, "getDemand", "()I");
        waitingDemandSupplyIdxMethod = env->GetMethodID(waitingDemandClass, "getSupplyIdx", "()Ljava/lang/String;");
        waitingDemandStartLocMethod = env->GetMethodID(waitingDemandClass, "getStartLoc", "()Lcom/ciel/microservices/dispatch_engine_service/mod_route/Location;");
        waitingDemandDestinationLocMethod = env->GetMethodID(waitingDemandClass, "getDestinationLoc", "()Lcom/ciel/microservices/dispatch_engine_service/mod_route/Location;");
        waitingDemandEtaToStartMethod = env->GetMethodID(waitingDemandClass, "getEtaToStart", "()Lcom/ciel/microservices/dispatch_engine_service/mod_route/TimeWindow;");
        waitingDemandEtaToDestinationMethod = env->GetMethodID(waitingDemandClass, "getEtaToDestination", "()Lcom/ciel/microservices/dispatch_engine_service/mod_route/TimeWindow;");

        newDemandClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/NewDemand");
        newDemandIdMethod = env->GetMethodID(newDemandClass, "getId", "()Ljava/lang/String;");
        newDemandDemandMethod = env->GetMethodID(newDemandClass, "getDemand", "()I");
        newDemandStartLocMethod = env->GetMethodID(newDemandClass, "getStartLoc", "()Lcom/ciel/microservices/dispatch_engine_service/mod_route/Location;");
        newDemandDestinationLocMethod = env->GetMethodID(newDemandClass, "getDestinationLoc", "()Lcom/ciel/microservices/dispatch_engine_service/mod_route/Location;");
        newDemandEtaToStartMethod = env->GetMethodID(newDemandClass, "getEtaToStart", "()Lcom/ciel/microservices/dispatch_engine_service/mod_route/TimeWindow;");

        demandActionClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/DemandAction");
        demandActionConstructor = env->GetMethodID(demandActionClass, "<init>", "(Ljava/lang/String;I)V");
        demandActionIdMethod = env->GetMethodID(demandActionClass, "getId", "()Ljava/lang/String;");
        demandActionDemandMethod = env->GetMethodID(demandActionClass, "getDemand", "()I");

        vehicleAssignedClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/VehicleAssigned");
        vehicleAssignedSupplyIdxMethod = env->GetMethodID(vehicleAssignedClass, "getSupplyIdx", "()Ljava/lang/String;");
        vehicleAssignedRouteOrderMethod = env->GetMethodID(vehicleAssignedClass, "getRouteOrder", "()Ljava/util/List;");
        vehicleAssignedRouteTimesMethod = env->GetMethodID(vehicleAssignedClass, "getRouteTimes", "()Ljava/util/List;");
        vehicleAssignedRouteDistancesMethod = env->GetMethodID(vehicleAssignedClass, "getRouteDistances", "()Ljava/util/List;");

        jclass objectClass = env->FindClass("java/lang/Object");
        equalsMethodID = env->GetMethodID(objectClass, "equals", "(Ljava/lang/Object;)Z");

        // OptimizeType 상수 로드
        jclass enumClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/OptimizeType");
        jfieldID enumFieldID = env->GetStaticFieldID(enumClass, "Time", "Lcom/ciel/microservices/dispatch_engine_service/mod_route/OptimizeType;");
        optimizeTypeTime = env->GetStaticObjectField(enumClass, enumFieldID);
        enumFieldID = env->GetStaticFieldID(enumClass, "Distance", "Lcom/ciel/microservices/dispatch_engine_service/mod_route/OptimizeType;");
        optimizeTypeDistance = env->GetStaticObjectField(enumClass, enumFieldID);
        enumFieldID = env->GetStaticFieldID(enumClass, "Co2", "Lcom/ciel/microservices/dispatch_engine_service/mod_route/OptimizeType;");
        optimizeTypeCo2 = env->GetStaticObjectField(enumClass, enumFieldID);

        enumClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/RouteType");
        enumFieldID = env->GetStaticFieldID(enumClass, "OSRM", "Lcom/ciel/microservices/dispatch_engine_service/mod_route/RouteType;");
        routeTypeOsrm = env->GetStaticObjectField(enumClass, enumFieldID);
        enumFieldID = env->GetStaticFieldID(enumClass, "Valhalla", "Lcom/ciel/microservices/dispatch_engine_service/mod_route/RouteType;");
        routeTypeValhalla = env->GetStaticObjectField(enumClass, enumFieldID);

        modDispatchSolutionClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/ModDispatchSolution");
        modDispatchSolutionConstructor = env->GetMethodID(modDispatchSolutionClass, "<init>", "(Ljava/util/List;Ljava/util/List;Ljava/util/List;JJ)V");

        modVehicleRouteClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/ModVehicleRoute");
        modVehicleRouteConstructor = env->GetMethodID(modVehicleRouteClass, "<init>", "(Ljava/lang/String;Ljava/util/List;)V");

        modRouteClass = env->FindClass("com/ciel/microservices/dispatch_engine_service/mod_route/ModRoute");
        modRouteConstructor = env->GetMethodID(modRouteClass, "<init>", "(Ljava/lang/String;ILcom/ciel/microservices/dispatch_engine_service/mod_route/Location;)V");
    }

    jobject convertToJobject(JNIEnv *env, const AlgorithmParameters &ap) {
        // java 객체 생성
        jobject algorithmParameters = env->NewObject(algorithmParametersClass, algorithmParameterConstructor);
        // java 객체 필드 값 설정
        env->SetIntField(algorithmParameters, nbIterationsField, ap.nb_iterations);
        env->SetIntField(algorithmParameters, timeLimitField, ap.time_limit);
        env->SetIntField(algorithmParameters, threadCountField, ap.thread_count);
        env->SetDoubleField(algorithmParameters, shawPhiDistanceField, ap.shaw_phi_distance);
        env->SetDoubleField(algorithmParameters, shawChiTimeField, ap.shaw_chi_time);
        env->SetDoubleField(algorithmParameters, shawPsiCapacityField, ap.shaw_psi_capacity);
        env->SetIntField(algorithmParameters, shawRemovalPField, ap.shaw_removal_p);
        env->SetIntField(algorithmParameters, worstRemovalPField, ap.worst_removal_p);
        env->SetDoubleField(algorithmParameters, simulatedAnnealingStartTempControlWField, ap.simulated_annealing_start_temp_control_w);
        env->SetDoubleField(algorithmParameters, simulatedAnnealingCoolingRateCField, ap.simulated_annealing_cooling_rate_c);
        env->SetDoubleField(algorithmParameters, adaptiveWeightAdjD1Field, ap.adaptive_weight_adj_d1);
        env->SetDoubleField(algorithmParameters, adaptiveWeightAdjD2Field, ap.adaptive_weight_adj_d2);
        env->SetDoubleField(algorithmParameters, adaptiveWeightAdjD3Field, ap.adaptive_weight_adj_d3);
        env->SetDoubleField(algorithmParameters, adaptiveWeightDacayRField, ap.adaptive_weight_dacay_r);
        env->SetDoubleField(algorithmParameters, insertionObjectiveNoiseNField, ap.insertion_objective_noise_n);
        env->SetDoubleField(algorithmParameters, removalReqIterationControlEField, ap.removal_req_iteration_control_e);
        env->SetDoubleField(algorithmParameters, delaytimePenaltyField, ap.delaytime_penalty);
        env->SetDoubleField(algorithmParameters, waittimePenaltyField, ap.waittime_penalty);
        env->SetIntField(algorithmParameters, seedField, ap.seed);
        env->SetBooleanField(algorithmParameters, enableMissingSolutionField, ap.enable_missing_solution);
        env->SetBooleanField(algorithmParameters, skipRemoveRouteField, ap.skip_remove_route);
        env->SetIntField(algorithmParameters, unfeasibleDelaytimeField, ap.unfeasible_delaytime);
        return algorithmParameters;
    }

    AlgorithmParameters convertToAlgorithmParameters(JNIEnv* env, const jobject &object) {
        AlgorithmParameters ap;
        ap.nb_iterations = env->GetIntField(object, nbIterationsField);
        ap.time_limit = env->GetIntField(object, timeLimitField);
        ap.thread_count = env->GetIntField(object, threadCountField);
        ap.shaw_phi_distance = env->GetDoubleField(object, shawPhiDistanceField);
        ap.shaw_chi_time = env->GetDoubleField(object, shawChiTimeField);
        ap.shaw_psi_capacity = env->GetDoubleField(object, shawPsiCapacityField);
        ap.shaw_removal_p = env->GetIntField(object, shawRemovalPField);
        ap.worst_removal_p = env->GetIntField(object, worstRemovalPField);
        ap.simulated_annealing_start_temp_control_w = env->GetDoubleField(object, simulatedAnnealingStartTempControlWField);
        ap.simulated_annealing_cooling_rate_c = env->GetDoubleField(object, simulatedAnnealingCoolingRateCField);
        ap.adaptive_weight_adj_d1 = env->GetDoubleField(object, adaptiveWeightAdjD1Field);
        ap.adaptive_weight_adj_d2 = env->GetDoubleField(object, adaptiveWeightAdjD2Field);
        ap.adaptive_weight_adj_d3 = env->GetDoubleField(object, adaptiveWeightAdjD3Field);
        ap.adaptive_weight_dacay_r = env->GetDoubleField(object, adaptiveWeightDacayRField);
        ap.insertion_objective_noise_n = env->GetDoubleField(object, insertionObjectiveNoiseNField);
        ap.removal_req_iteration_control_e = env->GetDoubleField(object, removalReqIterationControlEField);
        ap.delaytime_penalty = env->GetDoubleField(object, delaytimePenaltyField);
        ap.waittime_penalty = env->GetDoubleField(object, waittimePenaltyField);
        ap.seed = env->GetIntField(object, seedField);
        ap.enable_missing_solution = env->GetBooleanField(object, enableMissingSolutionField);
        ap.skip_remove_route = env->GetBooleanField(object, skipRemoveRouteField);
        ap.unfeasible_delaytime = env->GetIntField(object, unfeasibleDelaytimeField);
        return ap;
    }

    jobject convertToJobject(JNIEnv *env, const ModRouteConfiguration &conf) {
        jobject modRouteConfiguration = env->NewObject(modRouteConfigurationClass, modRouteConfigurationConstructor);
        env->SetIntField(modRouteConfiguration, maxDurationField, conf.nMaxDuration);
        env->SetIntField(modRouteConfiguration, bypassRatioField, conf.nBypassRatio);
        env->SetIntField(modRouteConfiguration, serviceTimeField, conf.nServiceTime);
        env->SetIntField(modRouteConfiguration, acceptableBufferField, conf.nAcceptableBuffer);
        env->SetBooleanField(modRouteConfiguration, logRequestField, conf.bLogRequest);
        env->SetIntField(modRouteConfiguration, cacheExpirationTimeField, conf.nCacheExpirationTime);
        env->SetIntField(modRouteConfiguration, solutionLimitField, conf.nSolutionLimit);
        return modRouteConfiguration;
    }

    ModRouteConfiguration convertToModRouteConfiguration(JNIEnv* env, const jobject &object) {
        ModRouteConfiguration conf;
        conf.nMaxDuration = env->GetIntField(object, maxDurationField);
        conf.nBypassRatio = env->GetIntField(object, bypassRatioField);
        conf.nServiceTime = env->GetIntField(object, serviceTimeField);
        conf.nAcceptableBuffer = env->GetIntField(object, acceptableBufferField);
        conf.bLogRequest = env->GetBooleanField(object, logRequestField);
        conf.nCacheExpirationTime = env->GetIntField(object, cacheExpirationTimeField);
        conf.nSolutionLimit = env->GetIntField(object, solutionLimitField);
        return conf;
    }

    Location convertToLocation(JNIEnv* env, const jobject &object) {
        Location location;
        location.lng = env->CallDoubleMethod(object, getLongitudeMethod);
        location.lat = env->CallDoubleMethod(object, getLatitudeMethod);
        location.waypoint_id = env->CallIntMethod(object, getWaypointIdMethod);
        jobject stationId = env->CallObjectMethod(object, getStationIdMethod);
        location.station_id = convertToString(env, stationId);
        env->DeleteLocalRef(stationId);
        location.direction = env->CallIntMethod(object, getDirectionMethod);
        return location;
    }

    jobject convertToJobject(JNIEnv *env, Location& location) {
        jobject station_id = convertToJstring(env, location.station_id);
        jobject result = env->NewObject(locationClass, locationConstructor, location.lng, location.lat, location.waypoint_id, station_id, location.direction);
        env->DeleteLocalRef(station_id);
        return result;
    }

    VehicleLocation convertToVehicleLocation(JNIEnv *env, const jobject &object) {
        VehicleLocation vehicleLocation;

        jobject supplyIdx = env->CallObjectMethod(object, getSupplyIdxMethod);
        vehicleLocation.supplyIdx = convertToString(env, supplyIdx);
        env->DeleteLocalRef(supplyIdx);
        vehicleLocation.capacity = env->CallIntMethod(object, getCapacityMethod);
        jobject location = env->CallObjectMethod(object, getLocationMethod);
        vehicleLocation.location = convertToLocation(env, location);
        env->DeleteLocalRef(location);

        return vehicleLocation;
    }

    OnboardDemand convertToOnboardDemand(JNIEnv *env, const jobject& object) {
        OnboardDemand demand;

        jobject id = env->CallObjectMethod(object, onboardDemandIdMethod);
        demand.id = convertToString(env, id);
        env->DeleteLocalRef(id);
        jobject supplyIdx = (jstring) env->CallObjectMethod(object, onboardDemandSupplyIdxMethod);
        demand.supplyIdx = convertToString(env, supplyIdx);
        env->DeleteLocalRef(supplyIdx);
        demand.demand = env->CallIntMethod(object, onboardDemandDemandMethod);
        jobject destinationLoc = env->CallObjectMethod(object, onboardDemandDestinationLocMethod);
        demand.destinationLoc = convertToLocation(env, destinationLoc);
        env->DeleteLocalRef(destinationLoc);
        jobject etaToDestination = env->CallObjectMethod(object, onboardDemandEtaToDestinationMethod);
        demand.etaToDestination[0] = env->CallIntMethod(etaToDestination, getLowMethod);
        demand.etaToDestination[1] = env->CallIntMethod(etaToDestination, getHighMethod);
        env->DeleteLocalRef(etaToDestination);

        return demand;
    }

    OnboardWaitingDemand convertToOnboardWaitingDemand(JNIEnv *env, const jobject& object) {
        OnboardWaitingDemand demand;

        jobject id = env->CallObjectMethod(object, waitingDemandIdMethod);
        demand.id = convertToString(env, id);
        env->DeleteLocalRef(id);
        jobject supplyIdx = env->CallObjectMethod(object, waitingDemandSupplyIdxMethod);
        demand.supplyIdx = convertToString(env, supplyIdx);
        env->DeleteLocalRef(supplyIdx);
        demand.demand = env->CallIntMethod(object, waitingDemandDemandMethod);
        jobject startLoc = env->CallObjectMethod(object, waitingDemandStartLocMethod);
        demand.startLoc = convertToLocation(env, startLoc);
        env->DeleteLocalRef(startLoc);
        jobject destinationLoc = env->CallObjectMethod(object, waitingDemandDestinationLocMethod);
        demand.destinationLoc = convertToLocation(env, destinationLoc);
        env->DeleteLocalRef(destinationLoc);
        jobject etaToStart = env->CallObjectMethod(object, waitingDemandEtaToStartMethod);
        demand.etaToStart[0] = env->CallIntMethod(etaToStart, getLowMethod);
        demand.etaToStart[1] = env->CallIntMethod(etaToStart, getHighMethod);
        env->DeleteLocalRef(etaToStart);
        jobject etaToDestination = env->CallObjectMethod(object, waitingDemandEtaToDestinationMethod);
        demand.etaToDestination[0] = env->CallIntMethod(etaToDestination, getLowMethod);
        demand.etaToDestination[1] = env->CallIntMethod(etaToDestination, getHighMethod);
        env->DeleteLocalRef(etaToDestination);

        return demand;
    }

    NewDemand convertToNewDemand(JNIEnv *env, const jobject& object) {
        NewDemand demand;

        jobject id = env->CallObjectMethod(object, newDemandIdMethod);
        demand.id = convertToString(env, id);
        env->DeleteLocalRef(id);
        demand.demand = env->CallIntMethod(object, newDemandDemandMethod);
        jobject startLoc = env->CallObjectMethod(object, newDemandStartLocMethod);
        demand.startLoc = convertToLocation(env, startLoc);
        env->DeleteLocalRef(startLoc);
        jobject destinationLoc = env->CallObjectMethod(object, newDemandDestinationLocMethod);
        demand.destinationLoc = convertToLocation(env, destinationLoc);
        env->DeleteLocalRef(destinationLoc);
        jobject etaToStart = env->CallObjectMethod(object, newDemandEtaToStartMethod);
        demand.etaToStart[0] = env->CallIntMethod(etaToStart, getLowMethod);
        demand.etaToStart[1] = env->CallIntMethod(etaToStart, getHighMethod);
        env->DeleteLocalRef(etaToStart);

        return demand;
    }

    std::pair<std::string, int> convertToDemandAction(JNIEnv *env, const jobject& object) {
        jobject demandActionId = env->CallObjectMethod(object, demandActionIdMethod);
        std::string id = convertToString(env, demandActionId);
        env->DeleteLocalRef(demandActionId);
        return std::make_pair(id, env->CallIntMethod(object, demandActionDemandMethod));
    }

    VehicleAssigned convertToVehicleAssigned(JNIEnv *env, const jobject& object) {
        VehicleAssigned vehicleAssigned;

        jobject supplyIdx = env->CallObjectMethod(object, vehicleAssignedSupplyIdxMethod);
        vehicleAssigned.supplyIdx = convertToString(env, supplyIdx);
        env->DeleteLocalRef(supplyIdx);
        jobject routeOrder = env->CallObjectMethod(object, vehicleAssignedRouteOrderMethod);
        int routeOrderSize = env->CallIntMethod(routeOrder, sizeMethod);
        for (int i = 0; i < routeOrderSize; i++) {
            jobject demandAction = env->CallObjectMethod(routeOrder, getMethodInList, i);
            vehicleAssigned.routeOrder.push_back(convertToDemandAction(env, demandAction));
            env->DeleteLocalRef(demandAction);
        }
        env->DeleteLocalRef(routeOrder);

        jobject routeTimes = env->CallObjectMethod(object, vehicleAssignedRouteTimesMethod);
        int routeTimeSize = env->CallIntMethod(routeTimes, sizeMethod);
        for (int i = 0; i < routeTimeSize; i++) {
            jobject longObject = env->CallObjectMethod(routeTimes, getMethodInList, i);
            int64_t value = env->CallLongMethod(longObject, longValueMethod);
            env->DeleteLocalRef(longObject);
            vehicleAssigned.routeTimes.push_back(value);
        }
        env->DeleteLocalRef(routeTimes);

        jobject routeDistances = env->CallObjectMethod(object, vehicleAssignedRouteDistancesMethod);
        int routeDistanceSize = env->CallIntMethod(routeDistances, sizeMethod);
        for (int i = 0; i < routeDistanceSize; i++) {
            jobject longObject = env->CallObjectMethod(routeDistances, getMethodInList, i);
            int64_t value = env->CallLongMethod(longObject, longValueMethod);
            env->DeleteLocalRef(longObject);
            vehicleAssigned.routeDistances.push_back(value);
        }
        env->DeleteLocalRef(routeDistances);
        
        return vehicleAssigned;
    }

    OptimizeType convertToOptimizeType(JNIEnv *env, const jobject& object) {
        if (env->CallBooleanMethod(object, equalsMethodID, optimizeTypeTime)) {
            return OptimizeType::Time;
        } else if (env->CallBooleanMethod(object, equalsMethodID, optimizeTypeDistance)) {
            return OptimizeType::Distance;
        } else if (env->CallBooleanMethod(object, equalsMethodID, optimizeTypeCo2)) {
            return OptimizeType::Co2;
        }
        return OptimizeType::Time;
    }

    RouteType convertToRouteType(JNIEnv *env, const jobject& object) {
        if (env->CallBooleanMethod(object, equalsMethodID, routeTypeOsrm)) {
            return RouteType::ROUTE_OSRM;
        } else if (env->CallBooleanMethod(object, equalsMethodID, routeTypeValhalla)) {
            return RouteType::ROUTE_VALHALLA;
        }
        return RouteType::ROUTE_VALHALLA;
    }

    std::string convertToString(JNIEnv *env, const jobject& object) {
        if (object == nullptr) {
            return "";
        }
        jstring s_object = (jstring)object;
        const char *str = env->GetStringUTFChars(s_object, nullptr);
        if (str == nullptr) {
            return "";
        }
        std::string result(str);
        env->ReleaseStringUTFChars(s_object, str);
        return result;
    }

    jstring convertToJstring(JNIEnv* env, const std::string& str) {
        return env->NewStringUTF(str.c_str());
    }

    ModRequest convertToModRequest(JNIEnv *env, const jobject& object) {
        ModRequest modRequest;
        jobject vehicleLocs = env->CallObjectMethod(object, getVehicleLocsMethod);
        int vehicleLocsSize = env->CallIntMethod(vehicleLocs, sizeMethod);
        for (int i = 0; i < vehicleLocsSize; i++) {
            jobject vehicleLoc = env->CallObjectMethod(vehicleLocs, getMethodInList, i);
            modRequest.vehicleLocs.push_back(convertToVehicleLocation(env, vehicleLoc));
            env->DeleteLocalRef(vehicleLoc);
        }
        env->DeleteLocalRef(vehicleLocs);
        jobject onboardDemands = env->CallObjectMethod(object, getOnboardDemandsMethod);
        int onboardDemandsSize = env->CallIntMethod(onboardDemands, sizeMethod);
        for (int i = 0; i < onboardDemandsSize; i++) {
            jobject onboardDemand = env->CallObjectMethod(onboardDemands, getMethodInList, i);
            modRequest.onboardDemands.push_back(convertToOnboardDemand(env, onboardDemand));
            env->DeleteLocalRef(onboardDemand);
        }
        env->DeleteLocalRef(onboardDemands);
        jobject onboardWaitingDemands = env->CallObjectMethod(object, getOnboardWaitingDemandsMethod);
        int onboardWaitingDemandsSize = env->CallIntMethod(onboardWaitingDemands, sizeMethod);
        for (int i = 0; i < onboardWaitingDemandsSize; i++) {
            jobject onboardWaitingDemand = env->CallObjectMethod(onboardWaitingDemands, getMethodInList, i);
            modRequest.onboardWaitingDemands.push_back(convertToOnboardWaitingDemand(env, onboardWaitingDemand));
            env->DeleteLocalRef(onboardWaitingDemand);
        }
        env->DeleteLocalRef(onboardWaitingDemands);
        jobject newDemands = env->CallObjectMethod(object, getNewDemandsMethod);
        int newDemandsSize = env->CallIntMethod(newDemands, sizeMethod);
        for (int i = 0; i < newDemandsSize; i++) {
            jobject newDemand = env->CallObjectMethod(newDemands, getMethodInList, i);
            modRequest.newDemands.push_back(convertToNewDemand(env, newDemand));
            env->DeleteLocalRef(newDemand);
        }
        env->DeleteLocalRef(newDemands);
        jobject assigned = env->CallObjectMethod(object, getAssignedMethod);
        int assignedSize = env->CallIntMethod(assigned, sizeMethod);
        for (int i = 0; i < assignedSize; i++) {
            jobject vehicleAssigned = env->CallObjectMethod(assigned, getMethodInList, i);
            modRequest.assigned.push_back(convertToVehicleAssigned(env, vehicleAssigned));
            env->DeleteLocalRef(vehicleAssigned);
        }
        env->DeleteLocalRef(assigned);
        jobject optimizeType = env->CallObjectMethod(object, getOptimizeTypeMethod);
        modRequest.optimizeType = convertToOptimizeType(env, optimizeType);
        env->DeleteLocalRef(optimizeType);
        modRequest.maxSolutions = env->CallIntMethod(object, getMaxSolutionsMethod);
        jobject locHash = env->CallObjectMethod(object, getLocHashMethod);
        modRequest.locHash = convertToString(env, locHash);
        env->DeleteLocalRef(locHash);
        jobject dateTime = env->CallObjectMethod(object, getDateTimeMethod);
        modRequest.dateTime = convertToString(env, dateTime);
        env->DeleteLocalRef(dateTime);

        return modRequest;
    }

    jobject convertToJobject(JNIEnv *env, ModRoute& modRoute) {
        jobject location = convertToJobject(env, modRoute.location);
        jobject id = convertToJstring(env, modRoute.id);
        jobject result = env->NewObject(modRouteClass, modRouteConstructor, id, modRoute.demand, location);
        env->DeleteLocalRef(id);
        env->DeleteLocalRef(location);
        return result;
    }

    jobject convertToJobject(JNIEnv *env, ModVehicleRoute& modVehicleRoute) {
        jobject routes = env->NewObject(arrayListClass, arrayListConstructor);
        for (int i = 0; i < modVehicleRoute.routes.size(); i++) {
            auto route = modVehicleRoute.routes[i];
            jobject modRoute = convertToJobject(env, route);
            env->CallBooleanMethod(routes, addMethod, modRoute);
            env->DeleteLocalRef(modRoute);
        }
        jobject supplyIdx = convertToJstring(env, modVehicleRoute.supply_idx);
        jobject result = env->NewObject(modVehicleRouteClass, modVehicleRouteConstructor, supplyIdx, routes);
        env->DeleteLocalRef(supplyIdx);
        env->DeleteLocalRef(routes);
        return result;
    }

    jobject convertToJobject(JNIEnv *env, ModDispatchSolution& solution) {
        jobject vehicleRoutes = env->NewObject(arrayListClass, arrayListConstructor);
        for (int i = 0; i < solution.vehicle_routes.size(); i++) {
            auto vehicle_route = solution.vehicle_routes[i];
            jobject modVehicleRoute = convertToJobject(env, vehicle_route);
            env->CallBooleanMethod(vehicleRoutes, addMethod, modVehicleRoute);
            env->DeleteLocalRef(modVehicleRoute);
        }

        jobject missing = env->NewObject(arrayListClass, arrayListConstructor);
        for (int i = 0; i < solution.missing.size(); i++) {
            auto route = solution.missing[i];
            jobject modRoute = convertToJobject(env, route);
            env->CallBooleanMethod(missing, addMethod, modRoute);
            env->DeleteLocalRef(modRoute);
        }

        jobject unacceptables = env->NewObject(arrayListClass, arrayListConstructor);
        for (int i = 0; i < solution.unacceptables.size(); i++) {
            auto route = solution.unacceptables[i];
            jobject modRoute = convertToJobject(env, route);
            env->CallBooleanMethod(missing, addMethod, modRoute);
            env->DeleteLocalRef(modRoute);
        }

        jobject result = env->NewObject(modDispatchSolutionClass, modDispatchSolutionConstructor, vehicleRoutes, missing, unacceptables, solution.total_distance, solution.total_time);
        env->DeleteLocalRef(unacceptables);
        env->DeleteLocalRef(missing);
        env->DeleteLocalRef(vehicleRoutes);
        return result;
    }

    jobject convertToJobject(JNIEnv *env, std::vector<ModDispatchSolution>& dispatch_solutions) {
        jobject dispathSolutionArray = env->NewObject(arrayListClass, arrayListConstructor);
        for (int i = 0; i < dispatch_solutions.size(); i++) {
            auto solution = dispatch_solutions[i];
            jobject dispatch_solution = convertToJobject(env, solution);
            env->CallBooleanMethod(dispathSolutionArray, addMethod, dispatch_solution);
            env->DeleteLocalRef(dispatch_solution);
        }
        return dispathSolutionArray;
    }

private:
    jclass algorithmParametersClass;
    jmethodID algorithmParameterConstructor;
    jfieldID nbIterationsField;
    jfieldID timeLimitField;
    jfieldID threadCountField;
    jfieldID shawPhiDistanceField;
    jfieldID shawChiTimeField;
    jfieldID shawPsiCapacityField;
    jfieldID shawRemovalPField;
    jfieldID worstRemovalPField;
    jfieldID simulatedAnnealingStartTempControlWField;
    jfieldID simulatedAnnealingCoolingRateCField;
    jfieldID adaptiveWeightAdjD1Field;
    jfieldID adaptiveWeightAdjD2Field;
    jfieldID adaptiveWeightAdjD3Field;
    jfieldID adaptiveWeightDacayRField;
    jfieldID insertionObjectiveNoiseNField;
    jfieldID removalReqIterationControlEField;
    jfieldID delaytimePenaltyField;
    jfieldID waittimePenaltyField;
    jfieldID seedField;
    jfieldID enableMissingSolutionField;
    jfieldID skipRemoveRouteField;
    jfieldID unfeasibleDelaytimeField;

    jclass modRouteConfigurationClass;
    jmethodID modRouteConfigurationConstructor;
    jfieldID maxDurationField;
    jfieldID bypassRatioField;
    jfieldID serviceTimeField;
    jfieldID acceptableBufferField;
    jfieldID logRequestField;
    jfieldID cacheExpirationTimeField;
    jfieldID solutionLimitField;

    jclass listClass;
    jmethodID sizeMethod;
    jmethodID getMethodInList;

    jclass arrayListClass;
    jmethodID arrayListConstructor;
    jmethodID addMethod;

    jclass longClass;
    jmethodID longValueMethod;

    jclass modRequestClass;
    jmethodID getVehicleLocsMethod;
    jmethodID getOnboardDemandsMethod;
    jmethodID getOnboardWaitingDemandsMethod;
    jmethodID getNewDemandsMethod;
    jmethodID getAssignedMethod;
    jmethodID getOptimizeTypeMethod;
    jmethodID getMaxSolutionsMethod;
    jmethodID getLocHashMethod;
    jmethodID getDateTimeMethod;

    jclass locationClass;
    jmethodID locationConstructor;
    jmethodID getLongitudeMethod;
    jmethodID getLatitudeMethod;
    jmethodID getWaypointIdMethod;
    jmethodID getStationIdMethod;
    jmethodID getDirectionMethod;

    jclass vehicleLocationClass;
    jmethodID getSupplyIdxMethod;
    jmethodID getCapacityMethod;
    jmethodID getLocationMethod;

    jclass timeWindowClass;
    jmethodID getLowMethod;
    jmethodID getHighMethod;

    jclass onboardDemandClass;
    jmethodID onboardDemandIdMethod;
    jmethodID onboardDemandDemandMethod;
    jmethodID onboardDemandSupplyIdxMethod;
    jmethodID onboardDemandDestinationLocMethod;
    jmethodID onboardDemandEtaToDestinationMethod;

    jclass waitingDemandClass;
    jmethodID waitingDemandIdMethod;
    jmethodID waitingDemandDemandMethod;
    jmethodID waitingDemandSupplyIdxMethod;
    jmethodID waitingDemandStartLocMethod;
    jmethodID waitingDemandDestinationLocMethod;
    jmethodID waitingDemandEtaToStartMethod;
    jmethodID waitingDemandEtaToDestinationMethod;

    jclass newDemandClass;
    jmethodID newDemandIdMethod;
    jmethodID newDemandDemandMethod;
    jmethodID newDemandStartLocMethod;
    jmethodID newDemandDestinationLocMethod;
    jmethodID newDemandEtaToStartMethod;

    jclass demandActionClass;
    jmethodID demandActionConstructor;
    jmethodID demandActionIdMethod;
    jmethodID demandActionDemandMethod;

    jclass vehicleAssignedClass;
    jmethodID vehicleAssignedSupplyIdxMethod;
    jmethodID vehicleAssignedRouteOrderMethod;
    jmethodID vehicleAssignedRouteTimesMethod;
    jmethodID vehicleAssignedRouteDistancesMethod;

    jmethodID equalsMethodID;

    jobject optimizeTypeTime;
    jobject optimizeTypeDistance;
    jobject optimizeTypeCo2;

    jobject routeTypeOsrm;
    jobject routeTypeValhalla;

    jclass modDispatchSolutionClass;
    jmethodID modDispatchSolutionConstructor;

    jclass modVehicleRouteClass;
    jmethodID modVehicleRouteConstructor;

    jclass modRouteClass;
    jmethodID modRouteConstructor;
};

JniConverter *g_conv;

JNIEXPORT void JNICALL Java_com_ciel_microservices_dispatch_1engine_1service_mod_1route_ModRouteEngine_initialize(
    JNIEnv *env,
    jclass cls)
{
    g_conv = new JniConverter(env);
}

JNIEXPORT jobject JNICALL Java_com_ciel_microservices_dispatch_1engine_1service_mod_1route_ModRouteEngine_runOptimize(
    JNIEnv *env,
    jobject obj,
    jobject jModRequest,
    jstring jRoutePath,
    jobject jRouteType,
    jint routeTasks,
    jstring rCachePath,
    jobject jAlgorithmParameters,
    jobject jModRouteConfiguration)
{
    auto modRequest = g_conv->convertToModRequest(env, jModRequest);
    auto routePath = g_conv->convertToString(env, jRoutePath);
    auto routeType = g_conv->convertToRouteType(env, jRouteType);
    auto cachePath = g_conv->convertToString(env, rCachePath);
    auto algorithmParameters = g_conv->convertToAlgorithmParameters(env, jAlgorithmParameters);
    auto modRouteConfiguration = g_conv->convertToModRouteConfiguration(env, jModRouteConfiguration);

    auto result = run_optimize(
        modRequest,
        routePath,
        routeType,
        routeTasks,
        cachePath,
        &algorithmParameters,
        modRouteConfiguration);

    return g_conv->convertToJobject(env, result);
}

JNIEXPORT void JNICALL Java_com_ciel_microservices_dispatch_1engine_1service_mod_1route_ModRouteEngine_clearCache(
    JNIEnv *env,
    jobject obj)
{
    clear_cache();
}

JNIEXPORT jobject JNICALL Java_com_ciel_microservices_dispatch_1engine_1service_mod_1route_ModRouteEngine_default_1algorithm_1parameters(
    JNIEnv *env,
    jobject obj)
{
    auto ap = default_algorithm_parameters();
    return g_conv->convertToJobject(env, ap);
}

JNIEXPORT jobject JNICALL Java_com_ciel_microservices_dispatch_1engine_1service_mod_1route_ModRouteEngine_default_1mod_1route_1configuration(
    JNIEnv *env,
    jobject obj)
{
    auto conf = default_mod_configuraiton();
    return g_conv->convertToJobject(env, conf);
}
