package com.ciel.microservices.dispatch_engine_service.mod_route;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;

public class ModRouteEngine {
    static {
        try {
            loadNativeLibrary();
        } catch (IOException e) {
            throw new RuntimeException("Failed to load native library", e);
        }
        initialize();
    }

    private static void loadNativeLibrary() throws IOException {
        // OS에 맞는 라이브러리 확장자를 포함하여 라이브러리 파일 이름 생성
        String libraryFileName = System.mapLibraryName("modroute_jni");

        // JAR 내부의 라이브러리 위치 (예: /native/lib/ 디렉토리)
        String resourcePath = "/native/lib/" + libraryFileName;

        // 라이브러리를 임시 디렉토리로 추출
        File tempFile = extractResourceToTemp(resourcePath);

        // 추출된 라이브러리 로드
        System.load(tempFile.getAbsolutePath());
    }

    private static File extractResourceToTemp(String resourcePath) throws IOException {
        // 리소스 파일을 임시 파일로 추출
        File tempFile = File.createTempFile("lib", resourcePath.substring(resourcePath.lastIndexOf('.')));
        tempFile.deleteOnExit();

        try (InputStream in = ModRouteEngine.class.getResourceAsStream(resourcePath);
             FileOutputStream out = new FileOutputStream(tempFile)) {
            byte[] buffer = new byte[1024];
            int bytesRead;
            while ((bytesRead = in.read(buffer)) != -1) {
                out.write(buffer, 0, bytesRead);
            }
        }

        return tempFile;
    }

    private static native void initialize();

    public native List<ModDispatchSolution> runOptimize(ModRequest modRequest,
                                                        String routePath,
                                                        RouteType routeType,
                                                        int routeTasks,
                                                        String cachePath,
                                                        AlgorithmParameters algorithmParameters,
                                                        ModRouteConfiguration modRouteConfiguration);
    public native void clearCache();
    public native AlgorithmParameters default_algorithm_parameters();
    public native ModRouteConfiguration default_mod_route_configuration();
}
