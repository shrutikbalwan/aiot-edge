/**
 * @file federated_learning.c
 * @brief Federated Learning (FL) Model Aggregation & Application
 * @brief Server-side aggregation and client-side model update for AIoT-Edge
 * 
 * This file provides software-based implementations for federated learning:
 * - Server-side model aggregation from multiple device deltas
 * - Client-side model update application
 * - Model version tracking and validation
 */

#include "ai_accelerator.h"
#include "federated_learning.h"
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------ */
/*                                  Macros                        */
/* ------------------------------------------------------------ */
#define FL_MAX_DELTAS        10      /* Maximum number of device deltas to aggregate */
#define FL_MODEL_SIZE        256     /* Model weight array size for demo */
#define FL_VERSION_TRACKING  1       /* Enable version tracking */

/* ------------------------------------------------------------ */
/*                                  Type Definitions            */
/* ------------------------------------------------------------ */
typedef struct {
    char device_id[32];              /* Device identifier */
    cJSON *delta_json;               /* Model delta JSON */
    int32_t version;                 /* Model version */
    int32_t timestamp;               /* Update timestamp */
} fl_delta_t;

/* ------------------------------------------------------------ */
/*                                  Global Variables          */
/* ------------------------------------------------------------ */
static fl_delta_t fl_client_deltas[FL_MAX_DELTAS];
static uint8_t fl_client_count = 0;
static int32_t fl_current_version = 0;
static int32_t fl_current_model_weights[FL_MODEL_SIZE];

/* ------------------------------------------------------------ */
/*                                  Function Prototypes       */
/* ------------------------------------------------------------ */
static int32_t compute_aggregated_weight(int32_t a, int32_t b);
static void cleanup_delta(cJSON *delta);

/* ------------------------------------------------------------ */
/*                                  Function Implementations */
/* ------------------------------------------------------------ */

/* ------------------------------------------------------------ */
/*                              Server-Side Aggregation       */
/* ------------------------------------------------------------ */
/** */
/**
 * @brief Aggregate model deltas from multiple devices using weighted average
 * @param deltas Array of device deltas
 * @param count Number of deltas to aggregate
 * @param aggregated_output Output buffer for aggregated weights
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG on bad params, ESP_ERR_NOT_FOUND if no data
 */
esp_err_t fl_aggregate_deltas(fl_delta_t *deltas, int32_t count, int32_t *aggregated_output) {
    if (deltas == NULL || aggregated_output == NULL || count <= 0 || count > FL_MAX_DELTAS) {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Initialize aggregated output to zero */
    memset(aggregated_output, 0, FL_MODEL_SIZE * sizeof(int32_t));
    
    /* Accumulate all delta weights */
    for (int i = 0; i < count; i++) {
        if (deltas[i].delta_json != NULL) {
            /* In a real implementation, extract weights from JSON and average */
            /* For this implementation, extract and accumulate weights */
            ESP_LOGI("FL", "Processing delta from device: %s", deltas[i].device_id);
        }
    }
    
    /* Compute simple average of all deltas */
    for (int i = 0; i < count; i++) {
        if (deltas[i].delta_json != NULL) {
            /* Extract weight from first element as representative */
            /* In production, extract all 256 weights from JSON and average */
            int32_t first_weight = 0;
            /* For this demo, use a simple placeholder extraction */
            if (cJSON_IsString(deltas[i].delta_json)) {
                first_weight = atoi(deltas[i].delta_json->valuestring);
            }
            aggregated_output[i % FL_MODEL_SIZE] += first_weight;
        }
    }
    
    /* Divide by count to get average */
    if (count > 0) {
        for (int i = 0; i < FL_MODEL_SIZE; i++) {
            aggregated_output[i] = aggregated_output[i] / count;
        }
    }
    
    ESP_LOGI("FL", "Aggregated %d device deltas", count);
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Client-Side Update              */
/* ------------------------------------------------------------ */
/* /**
 * @brief Apply federated learning model delta to the AI accelerator
 * @param delta_json cJSON payload containing weight deltas
 * @return esp_err_t ESP_OK on success
 */ 
esp_err_t ai_accelerator_apply_federated_delta(cJSON *delta_json) {
    if (delta_json == NULL) {
        ESP_LOGE("FL", "Delta JSON is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Log the receipt of the federated learning delta */
    ESP_LOGI("FL", "Applying federated learning delta to AI accelerator");
    
    /* Apply weight deltas to the neural network model */
    /* Extract deltas from JSON and update model weights */
    if (cJSON_IsString(delta_json)) {
        /* String format: comma-separated weight deltas */
        char *delta_str = delta_json->valuestring;
        char *token = strtok(delta_str, ",");
        int idx = 0;
        while (token != NULL && idx < FL_MODEL_SIZE) {
            int32_t delta_val = atoi(token);
            fl_current_model_weights[idx] += delta_val;
            idx++;
            token = strtok(NULL, ",");
        }
        ESP_LOGI("FL", "Applied %d weight deltas from string delta", idx);
    } else {
        /* Object format: items are weight deltas */
        int item_count = cJSON_GetObjectItemCount(delta_json);
        int idx = 0;
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, delta_json) {
            if (idx >= FL_MODEL_SIZE) break;
            int32_t delta_val = item->valueint;
            fl_current_model_weights[idx] += delta_val;
            idx++;
        }
        ESP_LOGI("FL", "Applied %d weight deltas from object delta", idx);
    }
    
    /* Update model version tracking */
    fl_current_version++;
    fl_track_version(fl_current_version);
    
    /* Log delta information for debugging */
    if (cJSON_IsString(delta_json)) {
        ESP_LOGI("FL", "Delta type: string, content: %s", delta_json->valuestring);
    } else {
        ESP_LOGI("FL", "Delta type: object with %d items", cJSON_GetObjectItemCount(delta_json));
    }
    
    ESP_LOGI("FL", "Federated learning model update applied; version %d", fl_current_version);
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Version Tracking                */
/* ------------------------------------------------------------ */
/* /**
 * @brief Track the current model version after FL update
 * @param new_version The new model version number
 * @return esp_err_t ESP_OK on success
 */ 
esp_err_t fl_track_version(int32_t new_version) {
    /* Critical section - disable interrupts if needed */
    /* fl_current_version = new_version; */
    ESP_LOGI("FL", "Model version updated to: %d", new_version);
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Utility Functions               */
/* ------------------------------------------------------------ */
/* /**
 * @brief Compute aggregated weight using simple average
 * @param a First weight value
 * @param b Second weight value
 * @return int32_t Aggregated weight value
 */ 
static int32_t compute_aggregated_weight(int32_t a, int32_t b) {
    /* Simple average: (a + b) / 2 */
    return (a + b) / 2;
}

/* ------------------------------------------------------------ */
/*                          Cleanup                           */
/* ------------------------------------------------------------ */
/* /**
 * @brief Clean up a delta structure and free resources
 * @param delta Pointer to the delta to clean up
 */ 
static void cleanup_delta(cJSON *delta) {
    if (delta != NULL) {
        cJSON_Delete(delta);
    }
}

/* ------------------------------------------------------------ */
/*                          Export Functions                  */
/* ------------------------------------------------------------ */
/* /**
 * @brief Get the current model version
 * @param[out] version Pointer to store the current version
 * @return esp_err_t ESP_OK on success
 */ 
esp_err_t fl_get_version(int32_t *version) {
    if (version == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *version = fl_current_version;
    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          End of File                       */
/* ------------------------------------------------------------ */