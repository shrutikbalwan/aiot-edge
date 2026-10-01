#ifndef MODEL_COMPRESSION_H
#define MODEL_COMPRESSION_H

#include <stdint.h>
#include <stddef.h>
#include "ai_accelerator.h"

/* ------------------------------------------------------------ */
/*                          Compression Statistics              */
/* ------------------------------------------------------------ */
typedef struct {
    uint16_t original_size;     /* Original number of weights */
    uint16_t pruned_size;       /* Pruned number of weights (zeros) */
    float prune_ratio;          /* Target sparsity ratio (0.0 - 1.0) */
    uint32_t macs_reduction;    /* Estimated MACs reduction percentage */
} compression_stats_t;

/* ------------------------------------------------------------ */
/*                                  Function Prototypes         */
/* ------------------------------------------------------------ */
/* Pruning Functions */
esp_err_t model_compression_prune(int8_t *weights, uint16_t size, float ratio,
                                 int8_t **pruned, uint16_t *pruned_size);

/* Huffman Compression Functions */
esp_err_t model_compression_huffman(int8_t *weights, uint16_t size,
                                   uint8_t **compressed, uint16_t *comp_size);

/* Report Functions */
void model_compression_print_report(void);
compression_stats_t* model_compression_get_stats(void);

/* ------------------------------------------------------------ */
/*                          Compression Parameters              */
/* ------------------------------------------------------------ */
#define PRUNE_THRESHOLD_DEFAULT  0.01f    /* Default small weight threshold */
#define HUFFMAN_TABLE_SIZE       256   /* Huffman code table size */
#define MIN_PRUNE_RATIO          0.1f   /* Minimum pruning ratio (10%) */
#define MAX_PRUNE_RATIO          0.8f   /* Maximum pruning ratio (80%) */

/* ------------------------------------------------------------ */
/*                          End of Header                         */
/* ------------------------------------------------------------ */
#endif /* MODEL_COMPRESSION_H */