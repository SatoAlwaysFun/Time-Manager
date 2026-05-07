#include "task.h"

const char *priority_to_string(Priority p) {
    switch (p) {
        case PRIORITY_HIGH:   return "Cao";
        case PRIORITY_MEDIUM: return "Trung bình";
        case PRIORITY_LOW:
        default:              return "Thấp";
    }
}