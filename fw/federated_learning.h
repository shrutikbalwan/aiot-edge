/**
 * @file federated_learning.h
 * @brief Federated Learning (FL) Header for AIoT-Edge
 * @brief Server-side aggregation and client-side model update declarations
 */

#ifndef FEDERATED_LEARNING_H
#define FEDERATED_LEARNING_H

#include <stdint.h>
#include "ai_accelerator.h"

/* ------------------------------------------------------------ */
/*                                  Type Definitions           */
#define FL_MAX_DELTAS        10      /* Maximum number of device deltas to aggregate */
#define FL_MODEL_SIZE        256     /* Model weight array size for demo */
#define FL_VERSION_TRACKING  1       /* Enable version tracking */

/* FL Delta Structure */
typedef struct {
    char device_id[32];              /* Device identifier */
    cJSON *delta_json;               /* Model delta JSON */
    int32_t version;                 /* Model version */
    int32_t timestamp;               /* Update timestamp */
} fl_delta_t;

/* ------------------------------------------------------------ */
/*                                  Macro Definitions           */
#define FL_MODEL_SIZE        256     /* Model weight array size for demo */
#define FL_VERSION_TRACKING  1       /* Enable version tracking */

/* ------------------------------------------------------------ */
/*                                  Function Declarations       */
/* ------------------------------------------------------------ */

/* ------------------------------------------------------------ */
/*                          Server-Side Aggregation           */
/* ------------------------------------------------------------ */
/* /**
 * @brief Aggregate model deltas from multiple devices using weighted average
 * @param deltas Array of device deltas
 * @param count Number of deltas to aggregate
 * @param aggregated_output Output buffer for aggregated weights
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG on bad params
 */
esp_err_t fl_aggregate_deltas(fl_delta_t *deltas, int32_t count, int32_t *aggregated_output);

/* ------------------------------------------------------------ */
/*                          Client-Side Update                */
/* ------------------------------------------------------------ */
/* /**
 * @brief Apply federated learning model delta to the AI accelerator
 * @param delta_json cJSON payload containing weight deltas
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG on bad params
 */
esp_err_t ai_accelerator_apply_federated_delta(cJSON *delta_json);

/* ------------------------------------------------------------ */
/*                          Version Tracking                  */
/* ------------------------------------------------------------ */
/* /**
 * @brief Get the current model version
 * @param[out] version Pointer to store the current version
 * @return esp_err_t ESP_OK on success
 */
esp_err_t fl_get_version(int32_t *version);

/* ------------------------------------------------------------ */
/*                          End of Header                       */
#endif /* FEDERATED_LEARNING_H */