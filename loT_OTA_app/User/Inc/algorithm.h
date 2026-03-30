#ifndef ALGORITHM_H_
#define ALGORITHM_H_

#define true 1
#define false 0
#define FS 100
#define BUFFER_SIZE  (FS* 5) 
#define HR_FIFO_SIZE 7
#define MA4_SIZE  4 // DO NOT CHANGE
#define HAMMING_SIZE  5// DO NOT CHANGE
#define min(x,y) ((x) < (y) ? (x) : (y))
#define ALPHA_SHIFT 10
#define FIFO_CAPACITY 1024 
typedef struct {
    uint32_t ir_buf[FIFO_CAPACITY];
    uint32_t red_buf[FIFO_CAPACITY];
    volatile uint32_t head; // 写指针
    volatile uint32_t tail; // 读指针
} PPG_FIFO_t;
void FIFO_Init(PPG_FIFO_t* fifo);
void FIFO_Push(PPG_FIFO_t* fifo, uint32_t ir, uint32_t red);
uint32_t FIFO_GetCount(PPG_FIFO_t* fifo);
void FIFO_PeekWindow(PPG_FIFO_t* fifo, uint32_t* out_ir, uint32_t* out_red, uint32_t window_size);
void FIFO_Slide(PPG_FIFO_t* fifo, uint32_t slide_step);

typedef struct {
    int32_t alpha_q10;
    int32_t result_q10;
    uint8_t initialized;
} IIR_Filter_Fixed;
extern IIR_Filter_Fixed HR_Filter;
extern IIR_Filter_Fixed SpO2_Filter;

int32_t Apply_IIR_Filter_Fixed(IIR_Filter_Fixed *f, int32_t new_val);

void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer ,  int32_t n_ir_buffer_length, uint32_t *pun_red_buffer ,   int32_t *pn_spo2, int8_t *pch_spo2_valid ,  int32_t *pn_heart_rate , int8_t  *pch_hr_valid);
void maxim_find_peaks( int32_t *pn_locs, int32_t *pn_npks,  int32_t *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num );
void maxim_peaks_above_min_height( int32_t *pn_locs, int32_t *pn_npks,  int32_t *pn_x, int32_t n_size, int32_t n_min_height );
void maxim_remove_close_peaks( int32_t *pn_locs, int32_t *pn_npks,   int32_t  *pn_x, int32_t n_min_distance );
void maxim_sort_ascend( int32_t *pn_x, int32_t n_size );
void maxim_sort_indices_descend(  int32_t  *pn_x, int32_t *pn_indx, int32_t n_size);

#endif /* ALGORITHM_H_ */