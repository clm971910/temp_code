#ifndef INCLUDE_HYPERLOGLOG_REDIS_H_
#define INCLUDE_HYPERLOGLOG_REDIS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hll_ctx_s hll_ctx_t;


#define HLL_DENSE     0       /* 稠密编码方式，占用 16K 内存 */
#define HLL_SPARSE    1       /* 稀疏编码方式, 创建时用 1.5K 内存，在元素不断加入后，自行决定何时转换为稠密编码方式 */


/**
 * 创建
 *      encoding：预计可能加入的元素大概率 >512 时，请直接选择 HLL_DENSE, 提升性能
 *                如果不能确定，请选择 HLL_SPARSE
 *
 *      返回 NULL:表示失败
 */
hll_ctx_t * hll_ctx_create( unsigned char encoding );

/** 释放 */
void  hll_ctx_free( hll_ctx_t * thiz );

/** 重置 */
void  hll_ctx_reset( hll_ctx_t * thiz );

/** 获得基数统计的值 */
uint64_t hll_ctx_count( hll_ctx_t * thiz );

/**
 * 添加一个新的值
 *      返回值：-1：表示失败 0:表示成功
 */
int hll_ctx_add( hll_ctx_t * thiz, const unsigned char * ele, int ele_len );

/** 获得序列化后的字节最大长度，用于提前准备内存 */
int hll_ctx_serial_maxBytes( hll_ctx_t * thiz );

/**
 * 序列化，内部不会检查buf的可写部分，请调用 hll_ctx_serial_maxBytes 进行保障
 *      返回值：-1:表示失败  >=0:表示实际输出的字节数
 */
int hll_ctx_serialize( hll_ctx_t * thiz, uint8_t * buf );

/**
 * 反序列化
 *     read_bytes: 将会返回反序列化 实际使用的字节数，辅助外部程序做后续动作
 *
 *     返回值 NULL：表示失败，备注：返回的对象 需要用户自行释放
 */
hll_ctx_t * hll_ctx_unSerialize( const uint8_t * src, int src_len, int * read_bytes );

/**
 * 合并
 *      返回值：-1：表示失败 0:表示成功
 */
int hll_ctx_merge( hll_ctx_t * thiz, hll_ctx_t * for_merge );

/**
 * 快速合并，不经过反序列化 直接合并
 *      返回值：-1：表示失败  >=0:实际使用的字节数
 */
int hll_ctx_fast_merge( hll_ctx_t * thiz, const uint8_t * src, int src_len );


#ifdef __cplusplus
}
#endif


#endif /* INCLUDE_HYPERLOGLOG_REDIS_H_ */
