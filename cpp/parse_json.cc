#include <chrono>
#include <stdio.h>
#include "json.hpp"

int parseLine(char* line){
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char* p = line;
    while (*p <'0' || *p > '9') p++;
    line[i-3] = '\0';
    i = atoi(p);
    return i;
}

int getCurrentMemoryUsage(){ //Note: this value is in KB!
    FILE* file = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
    return result;
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s json_file\n", argv[0]);
        return 1;
    }
    
    printf("Start parser\n");
    int startMemory = getCurrentMemoryUsage();
    printf("Current Memory Usage: %dKB\n", startMemory);
    const auto start = std::chrono::high_resolution_clock::now();
    json::Parser parser;
    const auto value = parser.ParseFromFile(argv[1]);

    if (!value.valid()) {
        printf("Failed to parse %s\n", argv[1]);
        return 2;
    }
    const std::chrono::duration<double> elapsed =
            std::chrono::high_resolution_clock::now() - start;
        printf("Parse Json file %s took %d ms.\n",
	    argv[1],  
            (int)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());

    int endMemory = getCurrentMemoryUsage();
    int delta = endMemory - startMemory;
    float deltaInGB = delta / 1000000.f;
    printf("Current Memory Usage: %dKB\n Usage = %dKB (%.2fGB)\n", endMemory, delta, deltaInGB);

    return 0;
}
