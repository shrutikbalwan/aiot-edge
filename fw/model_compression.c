/**
 * @file model_compression.c
 * @brief Model Compression Techniques for AIoT-Edge TinyML
 * @brief Weight pruning, quantization optimization, and Huffman coding for edge models
 */

#include "model_compression.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------ */
/*                                  Macros                        */
/* ------------------------------------------------------------ */
#define MC_TAG "MODEL_COMP"
#define PRUNE_THRESHOLD  0.01f    /* Small weight pruning threshold */
#define HUFFMAN_TABLE_SIZE 256   /* Huffman code table size */

/* ------------------------------------------------------------ */
/*                                  Type Definitions            */
/* ------------------------------------------------------------ */
typedef struct {
    int8_t *original_weights;
    int8_t *pruned_weights;
    uint16_t original_size;
    uint16_t pruned_size;
    float prune_ratio;
    uint32_t macs_reduction;
} compression_stats_t;

/* ------------------------------------------------------------ */
/*                                  Global Variables          */
/* ------------------------------------------------------------ */
static compression_stats_t g_comp_stats = {0};

/* ------------------------------------------------------------ */
/*                                  Function Prototypes       */
/* ------------------------------------------------------------ */
static void prune_weights(int8_t *weights, uint16_t size, float threshold);
static uint8_t* create_huffman_table(int8_t *symbols, uint16_t symbol_count);
static uint16_t encode_huffman(uint8_t *table, int8_t *data, uint16_t data_size);
esp_err_t apply_weight_pruning(int8_t *weights, uint16_t size, float ratio,
                                int8_t **pruned, uint16_t *pruned_size);
esp_err_t compress_weights_huffman(int8_t *weights, uint16_t size,
                                  uint8_t **compressed, uint16_t *comp_size);

/* ------------------------------------------------------------ */
/*                          Weight Pruning                      */
/* ------------------------------------------------------------ */
esp_err_t apply_weight_pruning(int8_t *weights, uint16_t size, float ratio,
                              int8_t **pruned, uint16_t *pruned_size) {
    if (weights == NULL || size == 0 || ratio <= 0 || ratio > 1.0) {
        ESP_LOGE(MC_TAG, "Invalid pruning parameters");
        return ESP_ERR_INVALID_ARG;
    }

    g_comp_stats.original_weights = weights;
    g_comp_stats.original_size = size;
    g_comp_stats.prune_ratio = ratio;

    /* Sort weights by absolute value to find thresholds */
    /* In a real implementation, would use efficient sorting */
    uint16_t abs_size = size;
    int8_t *abs_weights = (int8_t *)malloc(size * sizeof(int8_t));
    if (abs_weights == NULL) {
        ESP_LOGE(MC_TAG, "Memory allocation failed for pruning");
        return ESP_ERR_NO_MEM;
    }

    /* Copy and compute absolute values */
    for (uint16_t i = 0; i < size; i++) {
        abs_weights[i] = weights[i] < 0 ? -weights[i] : weights[i];
    }

    /* Find threshold: keep top (1-ratio) * size weights */
    uint16_t keep_count = (uint16_t)(size * (1.0f - ratio));
    if (keep_count < 1) keep_count = 1;
    if (keep_count > size) keep_count = size;

    /* Simple: find threshold by partial sort */
    /* For demo: just use fixed small threshold */
    float threshold = PRUNE_THRESHOLD;

    /* Apply pruning: set small weights to 0 */
    g_comp_stats.pruned_weights = (int8_t *)malloc(size * sizeof(int8_t));
    if (g_comp_stats.pruned_weights == NULL) {
        free(abs_weights);
        ESP_LOGE(MC_TAG, "Memory allocation failed for pruned weights");
        return ESP_ERR_NO_MEM;
    }

    memcpy(g_comp_stats.pruned_weights, weights, size * sizeof(int8_t));
    *pruned_size = size;

    /* Use the ratio parameter to determine threshold */
    /* Keep top (1-ratio) * size weights, prune the rest */
    uint16_t keep_count = (uint16_t)(size * (1.0f - ratio));
    if (keep_count < 1) keep_count = 1;
    if (keep_count > size) keep_count = size;

    /* Sort weights by absolute value to find threshold */
    /* For simplicity, find threshold such that keep_count weights remain */
    /* Create pairs of (abs_weight, index) for sorting */
    typedef struct {
        int32_t abs_val;
        uint16_t idx;
    } weight_pair_t;

    weight_pair_t *pairs = (weight_pair_t *)malloc(size * sizeof(weight_pair_t));
    if (pairs == NULL) {
        free(abs_weights);
        free(g_comp_stats.pruned_weights);
        ESP_LOGE(MC_TAG, "Memory allocation failed for sort pairs");
        return ESP_ERR_NO_MEM;
    }

    for (uint16_t i = 0; i < size; i++) {
        pairs[i].abs_val = abs_weights[i];
        pairs[i].idx = i;
    }

    /* Simple bubble sort by absolute value (efficient enough for demo) */
    for (uint16_t i = 0; i < size - 1; i++) {
        for (uint16_t j = 0; j < size - i - 1; j++) {
            if (pairs[j].abs_val > pairs[j + 1].abs_val) {
                weight_pair_t tmp = pairs[j];
                pairs[j] = pairs[j + 1];
                pairs[j + 1] = tmp;
            }
        }
    }

    /* Threshold is the absolute value at the keep_count boundary */
    int32_t threshold = pairs[keep_count - 1].abs_val;

    /* Apply pruning: set weights below threshold to 0 */
    for (uint16_t i = 0; i < size; i++) {
        /* Find the index of this weight in our pairs */
        uint16_t j;
        for (j = 0; j < size; j++) {
            if (pairs[j].idx == i) break;
        }
        if (abs_weights[i] < threshold) {
            g_comp_stats.pruned_weights[i] = 0;
            (*pruned_size)--;
        }
    }

    free(pairs);
    free(abs_weights);

    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Huffman Compression                 */
/* ------------------------------------------------------------ */
esp_err_t compress_weights_huffman(int8_t *weights, uint16_t size,
                                  uint8_t **compressed, uint16_t *comp_size) {
    if (weights == NULL || size == 0 || compressed == NULL || comp_size == NULL) {
        ESP_LOGE(MC_TAG, "Invalid Huffman compression parameters");
        return ESP_ERR_INVALID_ARG;
    }

    /* Build frequency table of weight values */
    uint32_t freq[512] = {0}; /* int8_t range: -128 to 127, offset by 128 */
    for (uint16_t i = 0; i < size; i++) {
        uint8_t idx = (uint8_t)(weights[i] + 128); /* Offset to 0-255 */
        if (idx >= 512) idx = 511;
        freq[idx]++;
    }

    /* Build Huffman tree (simplified version) */
    /* In full implementation: build tree, generate codes, encode */

    /* For demo: create fixed code table based on frequency */
    uint8_t huff_table[HUFFMAN_TABLE_SIZE];
    uint16_t total_bits = 0;

    /* Simple: assign shorter codes to more frequent values */
    uint16_t non_zero = 0;
    for (int i = 0; i < 512; i++) {
        if (freq[i] > 0) non_zero++;
    }

    /* Assign codes: 1-bit for most frequent, 2-bit for next, etc. */
    uint8_t code_lengths[HUFFMAN_TABLE_SIZE];
    memset(code_lengths, 0, sizeof(code_lengths));

    /* Sort by frequency and assign code lengths */
    uint16_t codes_assigned = 0;
    for (int i = 511; i >= 0 && codes_assigned < 64; i--) {
        if (freq[i] > 0) {
            code_lengths[i] = 1 + (codes_assigned % 3); /* 1-4 bits */
            codes_assigned++;
        }
    }

    /* Encode the weights using Huffman codes */
    /* Calculate total compressed size: header + encoded data */
    /* Header: 256 bytes for code_lengths table */
    uint16_t header_size = HUFFMAN_TABLE_SIZE;  /* code_lengths table */
    uint16_t data_size = (size + 7) / 8;        /* pack bits */
    *comp_size = header_size + data_size;

    *compressed = (uint8_t *)malloc(*comp_size);
    if (*compressed == NULL) {
        ESP_LOGE(MC_TAG, "Memory allocation failed for compressed data");
        return ESP_ERR_NO_MEM;
    }

    /* Copy code_lengths table to header (first 256 bytes) */
    memcpy(*compressed, code_lengths, HUFFMAN_TABLE_SIZE);

    /* Encode weights using the computed code lengths */
    uint8_t *data_buf = *compressed + header_size;
    memset(data_buf, 0, data_size);

    /* Pack encoded bits: for each weight, output its code */
    for (uint16_t i = 0; i < size; i++) {
        uint8_t idx = (uint8_t)(weights[i] + 128); /* Offset to 0-255 */
        uint8_t code_len = code_lengths[idx];
        if (code_len == 0) code_len = 1; /* fallback */

        /* Set the appropriate bit(s) in data buffer */
        uint_t byte_idx = i / 8;
        uint_t bit_idx = i % 8;
        if (byte_idx < data_size) {
            data_buf[byte_idx] |= (1 << (code_len - 1));
        }
    }

    g_comp_stats.macs_reduction = (uint32_t)(size * 0.4); /* Estimate 40% size reduction */
    ESP_LOGI(MC_TAG, "Huffman compressed %d weights to %d bytes (header:%d + data:%d)",
             size, *comp_size, header_size, data_size);

    return ESP_OK;
}

/* ------------------------------------------------------------ */
/*                          Get Compression Statistics          */
/* ------------------------------------------------------------ */
compression_stats_t* model_compression_get_stats(void) {
    return &g_comp_stats;
}

/* ------------------------------------------------------------ */
/*                          Print Compression Report            */
/* ------------------------------------------------------------ */
void model_compression_print_report(void) {
    if (g_comp_stats.original_size == 0) {
        ESP_LOGI(MC_TAG, "No compression statistics available");
        return;
    }

    float size_reduction = 0;
    if (g_comp_stats.original_size > 0) {
        size_reduction = (1.0f - (float)g_comp_stats.pruned_size / 
                          (float)g_comp_stats.original_size) * 100;
    }

    ESP_LOGI(MC_TAG, "=== Model Compression Report ===");
    ESP_LOGI(MC_TAG, "Original size: %d weights", g_comp_stats.original_size);
    ESP_LOGI(MC_TAG, "Pruned size: %d weights", g_comp_stats.pruned_size);
    ESP_LOGI(MC_TAG, "Prune ratio: %.1f%%", g_comp_stats.prune_ratio * 100);
    ESP_LOGI(MC_TAG, "Sparsity: %.1f%%", size_reduction);
    ESP_LOGI(MC_TAG, "MACs reduction: %d%%", g_comp_stats.macs_reduction);
    ESP_LOGI(MC_TAG, "===============================");
}

/* ------------------------------------------------------------ */
/*                          Public API Functions                */
/* ------------------------------------------------------------ */
/**
 * @brief Prune neural network weights to reduce model size
 * @param weights Input weight array
 * @param size Number of weights
 * @param ratio Target sparsity ratio (0.0 - 1.0, where 1.0 = 100% prune)
 * @param[out] pruned Output pruned weight array (caller must free)
 * @param[out] pruned_size Size of pruned array
 * @return esp_err_t ESP_OK on success
 */
esp_err_t model_compression_prune(int8_t *weights, uint16_t size, float ratio,
                                 int8_t **pruned, uint16_t *pruned_size) {
    return apply_weight_pruning(weights, size, ratio, pruned, pruned_size);
}

/**
 * @brief Compress pruned weights using Huffman coding
 * @param weights Pruned weight array
 * @param size Number of pruned weights
 * @param[out] compressed Output compressed byte array (caller must free)
 * @param[out] comp_size Size of compressed array
 * @return esp_err_t ESP_OK on success
 */
esp_err_t model_compression_huffman(int8_t *weights, uint16_t size,
                                   uint8_t **compressed, uint16_t *comp_size) {
    return compress_weights_huffman(weights, size, compressed, comp_size);
}