#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "hyperloglog.h"


#pragma pack(1)
typedef struct hll_regi_s
{
    uint16_t  index;                                            /* 桶号 */
    uint8_t   count;                                            /* 值 */
} hll_regi_t;
#pragma pack()


struct hll_ctx_s
{
    uint8_t     encoding;                                       /* HLL_DENSE or HLL_SPARSE. */
    int         ele_num;
    uint8_t   * registers;

    /* 总长 512*sizeof(hll_regi_t) ，
     * 拆分成32个子区域，每条放16个元素，只要有一个满了，就转成稠密
     * 假设：hash值是很均匀的 */
    hll_regi_t * regi_arr;
};


#define HLL_P                       14                          /* The greater is P, the smaller the error. */
#define HLL_Q                       50                          /* (64-HLL_P) The number of bits of the hash value used for
                                                                    determining the number of leading zeros. */
#define HLL_REGISTERS               16384                       /* (1<<HLL_P) With P=14, 16384 registers. */
#define HLL_P_MASK                  16383                       /* (HLL_REGISTERS-1) Mask to index register. */

#define HLL_BITS                    6                           /* Enough to count up to 63 leading zeroes. */
#define HLL_REGISTER_MAX            63                          /* ((1<<HLL_BITS)-1) */

#define HLL_ALPHA_INF               0.721347520444481703680     /* constant for 0.5/ln(2) */

#define HLL_SERIAL_SPARSE_MIN       3000
#define HLL_SERIAL_SPARSE_BYTES     6144
#define HLL_SERIAL_DENSE_BYTES      HLL_REGISTERS               /* HLL_REGISTERS * 6 / 8 = 12288 */

#define HLL_MAGIC                   "HLL"
#define HLL_MAGIC_BYTES             3

#define VARINT32_MAX_BYTES          5

#define HLL_REGI_MAX                512                         /* 总共512个位置 */
#define HLL_REGI_MAX_BYTES          1536                        /* 总共 1.5K */
#define HLL_REGI_AREA_NUM           16                          /* 拆成16个子区域 */
#define HLL_REGI_AREA_SIZE          32                          /* 每个区域32个位置 */



/** 私有inline函数，提前做一下声明 */
int varint_decode_uint32( const uint8_t * buffer, uint32_t * value );
int varint_encode_uint32 ( uint32_t value, uint8_t * target );
uint64_t MurmurHash64A( const uint8_t * data, int len );


inline uint64_t
MurmurHash64A( const uint8_t * data, int len )
{
    register const uint64_t   m   = 0xc6a4a7935bd1e995ULL;
    register       uint64_t   h   = 0xadc83b19ULL ^ (len * m);
             const uint8_t  * end = data + (len-(len&7));

    while ( data != end )
    {
        register uint64_t k = *((uint64_t*)data);

        k *= m;
        k ^= k >> 47;
        k *= m;

        h ^= k;
        h *= m;

        data += 8;
    }

    switch ( len & 7 )
    {
        case 7: h ^= (uint64_t)data[6] << 48;
        case 6: h ^= (uint64_t)data[5] << 40;
        case 5: h ^= (uint64_t)data[4] << 32;
        case 4: h ^= (uint64_t)data[3] << 24;
        case 3: h ^= (uint64_t)data[2] << 16;
        case 2: h ^= (uint64_t)data[1] << 8;
        case 1: h ^= (uint64_t)data[0];
                h *= m;
    };

    h ^= h >> 47;
    h *= m;
    h ^= h >> 47;

    return h;
}


/**
 * 将一个uint32整数 做 varint 编码 输出到 buf中
 *
 * @param value       输出的值
 * @param buf         输出的缓冲 , 需确保buf 空间是够用的
 *
 * @return  写的字节数
 */
inline int
varint_encode_uint32 ( uint32_t value, uint8_t * buf )
{
    register uint8_t * target = buf;

    target[0] = (uint8_t)(value | 0x80);

    if ( value >= (1 << 7) )
    {
        target[1] = (uint8_t)( (value >>  7) | 0x80 );

        if ( value >= (1 << 14) )
        {
            target[2] = (uint8_t)( (value >> 14) | 0x80 );

            if ( value >= (1 << 21) )
            {
                target[3] = (uint8_t)((value >> 21) | 0x80);

                if ( value >= (1 << 28) )
                {
                    target[4] = (uint8_t)(value >> 28);
                    return 5;
                }
                else
                {
                    target[3] &= 0x7F;
                    return 4;
                }
            }
            else
            {
                target[2] &= 0x7F;
                return 3;
            }
        }
        else
        {
            target[1] &= 0x7F;
            return 2;
        }
    }
    else
    {
        target[0] &= 0x7F;
        return 1;
    }
}



/**
 * 从buf中 将 varint压缩编码的值 还原读取出来
 * 需要确保输入的buf 从 输出的指针到结尾 超过  5个byte, 避免出现core
 * 函数内部不做边界检查
 *
 * @param buffer    输入的buf
 * @param value     输出的值
 *
 * @return  序列化的字节数
 */
inline int
varint_decode_uint32( const uint8_t * buffer, uint32_t * value )
{
    register const uint8_t * ptr = buffer;

    uint32_t  b;
    uint32_t  result;

    b = *(ptr++); result  = (b & 0x7F)      ; if (!(b & 0x80)) goto done;
    b = *(ptr++); result |= (b & 0x7F) <<  7; if (!(b & 0x80)) goto done;
    b = *(ptr++); result |= (b & 0x7F) << 14; if (!(b & 0x80)) goto done;
    b = *(ptr++); result |= (b & 0x7F) << 21; if (!(b & 0x80)) goto done;
    b = *(ptr++); result |=  b         << 28; if (!(b & 0x80)) goto done;

    for ( uint32_t i = 0; i < VARINT32_MAX_BYTES; i++ )
    {
        b = *(ptr++); if (!(b & 0x80)) goto done;
    }

    return 0;

done:
    *value = result;
    return (ptr - buffer);
}



double hllSigma( double x )
{
    if (x == 1.) return INFINITY;

    double zPrime;
    double y = 1;
    double z = x;

    do {
        x *= x;
        zPrime = z;
        z += x * y;
        y += y;
    } while (zPrime != z);

    return z;
}


double hllTau( double x )
{
    if (x == 0. || x == 1.) return 0.;

    double zPrime;
    double y = 1.0;
    double z = 1 - x;

    do {
        x = sqrt(x);
        zPrime = z;
        y *= 0.5;
        z -= pow(1 - x, 2) * y;
    } while(zPrime != z);

    return z/3;
}


int hll_ctx_regi_arr_to_registers( uint8_t * registers, hll_regi_t * regi_arr )
{
    int  ele_num = 0;

    for ( int i = 0; i < HLL_REGI_MAX; i++ )
    {
        hll_regi_t curr = regi_arr[ i ];

        if ( curr.count == 0 )             continue;       /* count 一定是大于0的 */
        if ( curr.index >= HLL_REGISTERS ) continue;
        if ( curr.count <= registers[ curr.index ] ) continue;

        registers[ curr.index ] = curr.count;
        ele_num++;
    }

    return ele_num;
}


int hll_ctx_sparse_to_dense( hll_ctx_t * thiz )
{
    if ( thiz->encoding == HLL_DENSE ) return 0;
    if ( thiz->regi_arr == NULL )      return -1;

    // 准备一下内存
    if ( NULL != thiz->registers )
    {
        memset( thiz->registers, 0, HLL_REGISTERS );
    }
    else
    {
        thiz->registers = (uint8_t *)calloc( HLL_REGISTERS, sizeof(uint8_t) );
        if ( NULL == thiz->registers ) goto failed;
    }

    // 把已经有的 转换到 registers 里面
    hll_regi_t * regi_arr = thiz->regi_arr;

    thiz->ele_num  = hll_ctx_regi_arr_to_registers( thiz->registers, regi_arr );
    thiz->encoding = HLL_DENSE;
    thiz->regi_arr = NULL;

    free( regi_arr );                                       /* 释放 regi_arr */
    return 0;

failed:
    return -1;
}




int hll_ctx_add( hll_ctx_t * thiz, const unsigned char * ele, int ele_len )
{
    uint64_t hash, index;

    hash = MurmurHash64A( ele, ele_len );

    index =   hash & HLL_P_MASK;          /* Register index. */
    hash  >>= HLL_P;                      /* Remove bits used to address the register. */
    hash  |=  ((uint64_t)1<<HLL_Q);       /* Make sure the loop terminates
                                             and count will be <= Q+1. */
    uint64_t  count = 1;                  /* Initialized to 1 since we count the "00000...1" pattern. */

#if __x86_64__
    __asm__(
            "bsfq %1, %0\n\t"
            "jnz 1f\n\t"
            "movq $-1,%0\n\t"
            "1:"
            :"=q"(count):"q"(hash) );
    count++;
#else
    uint64_t bit = 1ULL;

    while ( ( hash & bit) == 0)
    {
        count++;
        bit <<= 1;
    }
#endif


set_regi:
    if ( HLL_DENSE == thiz->encoding )                      // 稠密编码
    {
        uint8_t * registers = thiz->registers;
        uint8_t   oldcount = registers[ index ];

        if ( count > oldcount )
        {
            registers[ index ] = count;
            thiz->ele_num += 1;                             // 这里的值，基于hash是很平均的，表达桶被设置值的次数
        }
    }
    else                                                    // 稀疏编码
    {
        hll_regi_t * regi_arr = thiz->regi_arr;

        regi_arr += ( index % HLL_REGI_AREA_NUM ) * HLL_REGI_AREA_SIZE;  // 计算出对应的子区域的第一个起始位置

        int i = 0;
        for ( ; i < HLL_REGI_AREA_SIZE; i++ )               // 顺序检测这32个位置
        {
            hll_regi_t curr = regi_arr[ i ];

            if ( curr.count == 0 )                          // 空的，追加
            {
                regi_arr[ i ].index = index;
                regi_arr[ i ].count = count;
                break;
            }

            if ( curr.index == index )                      // 非空 判断index
            {
                if ( curr.count < count )  regi_arr[ i ].count = count;
                break;
            }
        }

        if ( i == HLL_REGI_AREA_SIZE )                      // 如果32个位置都满了，这时候需要转换编码方式。
        {
            if ( -1 == hll_ctx_sparse_to_dense( thiz ) )
                return -1;

            goto set_regi;
        }
    }

    return 0;
}



uint64_t hll_ctx_count( hll_ctx_t * thiz )
{
    if ( thiz == NULL ) return 0ULL;

    int reghisto[ HLL_Q + 2 ] = {0};

    if ( thiz->encoding == HLL_DENSE )                      // 稠密编码
    {
        register uint8_t * registers = thiz->registers;

        for ( long i = 0; i < 1024; i++ )                   // 1024 * 16 = HLL_REGISTERS
        {
            reghisto[ registers[ 0 ] ]++;
            reghisto[ registers[ 1 ] ]++;
            reghisto[ registers[ 2 ] ]++;
            reghisto[ registers[ 3 ] ]++;
            reghisto[ registers[ 4 ] ]++;
            reghisto[ registers[ 5 ] ]++;
            reghisto[ registers[ 6 ] ]++;
            reghisto[ registers[ 7 ] ]++;
            reghisto[ registers[ 8 ] ]++;
            reghisto[ registers[ 9 ] ]++;
            reghisto[ registers[ 10 ] ]++;
            reghisto[ registers[ 11 ] ]++;
            reghisto[ registers[ 12 ] ]++;
            reghisto[ registers[ 13 ] ]++;
            reghisto[ registers[ 14 ] ]++;
            reghisto[ registers[ 15 ] ]++;

            registers += 16;
        }
    }
    else                                                        // 稀疏编码
    {
        register  hll_regi_t * regi_arr = thiz->regi_arr;

        for ( long i = 0; i < 128; i++ )                        // 128 * 4 = HLL_REGI_MAX
        {
            reghisto[ regi_arr[ 0 ].count ]++;
            reghisto[ regi_arr[ 1 ].count ]++;
            reghisto[ regi_arr[ 2 ].count ]++;
            reghisto[ regi_arr[ 3 ].count ]++;

            regi_arr += 4;
        }

        reghisto[0] += ( HLL_REGISTERS - HLL_REGI_MAX );
    }

    double E;
    register double m = HLL_REGISTERS;
    register double z = m * hllTau( (m - reghisto[ HLL_Q + 1 ]) / (double)m );

    for (int j = HLL_Q; j >= 1; --j) {
        z += reghisto[j];
        z *= 0.5L;
    }

    z += m * hllSigma( reghisto[0] / (double)m );
    E = llroundl( HLL_ALPHA_INF * m * m / z );

    return (uint64_t) E;
}



hll_ctx_t * hll_ctx_create( unsigned char encoding )
{
    if ( encoding != HLL_DENSE && encoding != HLL_SPARSE )
        return NULL;

    hll_ctx_t * thiz = (hll_ctx_t *)calloc( 1, sizeof(hll_ctx_t) );

    thiz->ele_num    = 0;
    thiz->encoding   = encoding;
    thiz->registers  = NULL;
    thiz->regi_arr = NULL;

    if ( HLL_SPARSE == encoding )
    {
        thiz->regi_arr = (hll_regi_t *)calloc( HLL_REGI_MAX, sizeof(hll_regi_t) );
        if ( NULL == thiz->regi_arr ) goto failed;
    }

    if ( HLL_DENSE == encoding )
    {
        thiz->registers = (uint8_t *)calloc( HLL_REGISTERS, sizeof(uint8_t) );
        if ( NULL == thiz->registers ) goto failed;
    }

    return thiz;

failed:
    if ( NULL != thiz->regi_arr )   free( thiz->regi_arr );
    if ( NULL != thiz->registers )  free( thiz->registers );
    if ( NULL != thiz )             free( thiz );

    return NULL;
}


void  hll_ctx_free( hll_ctx_t * thiz )
{
    if ( thiz == NULL ) return;

    if ( NULL != thiz->registers ) free( thiz->registers );
    if ( NULL != thiz->regi_arr )  free( thiz->regi_arr );

    free( thiz );
}


void  hll_ctx_reset( hll_ctx_t * thiz )
{
    if ( thiz == NULL ) return;

    thiz->ele_num = 0;

    if ( NULL != thiz->registers )
        memset( thiz->registers, 0, HLL_REGISTERS );

    if ( NULL != thiz->regi_arr )
        memset( thiz->regi_arr, 0, HLL_REGI_MAX_BYTES );
}



int hll_ctx_serial_maxBytes( hll_ctx_t * thiz )
{
    if ( NULL == thiz ) return -1;

    int bytes = 0;

    if ( NULL != thiz->registers )                          /* 稠密 */
    {
        if ( thiz->ele_num < HLL_SERIAL_SPARSE_MIN )        /* 经验值 */
        {
            bytes = HLL_SERIAL_SPARSE_BYTES;
        }
        else
        {
            bytes = HLL_SERIAL_DENSE_BYTES;
        }
    }
    else
    {
        if ( NULL != thiz->regi_arr )                       /* 稀疏 */
            bytes = HLL_REGI_MAX_BYTES;
    }

    bytes += sizeof( uint8_t );
    bytes += VARINT32_MAX_BYTES;
    bytes += HLL_MAGIC_BYTES;

    return bytes;
}



int hll_ctx_serialize( hll_ctx_t * thiz, uint8_t * buf )
{
    if ( NULL == thiz || NULL == buf ) return -1;

    int       write_len = 0;
    uint8_t * registers = thiz->registers;

    buf[ 0 ] = 'H'; buf[ 1 ] = 'L'; buf[ 2 ] = 'L';
    write_len += HLL_MAGIC_BYTES;

    buf[ write_len ] = thiz->encoding;                          // 输出编码
    write_len += 1;

    // 写入个数
    write_len += varint_encode_uint32 ( thiz->ele_num, buf + write_len );

    // 区分不同编码 分别处理
    if ( HLL_SPARSE == thiz->encoding )
    {
        // TODO: 这里有个潜在的改进是把 每个小区域具体有多少个值 先存起来
        memcpy( buf + write_len, thiz->regi_arr, HLL_REGI_MAX_BYTES );
        write_len += HLL_REGI_MAX_BYTES;

        return write_len;
    }

    if ( thiz->ele_num < HLL_SERIAL_SPARSE_MIN )
    {
        /** 转换成 val:num;val:num;val:num的格式，注意：不是 index:val;index:val的格式 ,
         *  原因是:
         *      超过128的index在varint中基本都要2个字节来表示, 如果用index:val的方式，一个pair需要2个字节
         *      num超过128是很少见的，一般1个字节就够了，用val:num的方式，这样一个pair只需要2个字节
         */
        uint8_t  val = registers[ 0 ];
        int      num = 1;

        for ( int i = 1; i < HLL_REGISTERS; i++ )
        {
            uint8_t cur = registers[ i ];

            if ( val == cur )
            {
                num++;
                continue;
            }
            else
            {
                buf[ write_len ] = val;                 // 输出值
                write_len += 1;

                write_len += varint_encode_uint32( num, buf + write_len );

                val = cur;
                num = 1;
            }
        }

        // 写个末尾的小哨兵。因为64是不可能出现的 val 值
        buf[ write_len ] = ( 1 << HLL_BITS );
        write_len += 1;
    }
    else
    {
        /** 从16K->12K 感觉没有大的价值，先不弄了 */
        memcpy( buf + write_len, thiz->registers, HLL_REGISTERS );
        write_len += ( HLL_REGISTERS );
    }

    return write_len;
}



int hll_ctx_merge_registers( uint8_t * dst, uint8_t * src )
{
    int ele_num0 = 0;
    int ele_num1 = 0;
    int ele_num2 = 0;
    int ele_num3 = 0;

    for ( long i = 0; i < HLL_REGISTERS;  )
    {
        uint8_t max0 = dst[ i ];
        uint8_t val0 = src[ i ];

        if ( val0 > max0 )
        {
            dst[ i ] = val0;
            ele_num0 += 1;
        }

        uint8_t max1 = dst[ i + 1 ];
        uint8_t val1 = src[ i + 1 ];

        if ( val1 > max1 )
        {
            dst[ i + 1 ] = val1;
            ele_num1 += 1;
        }

        uint8_t max2 = dst[ i + 2 ];
        uint8_t val2 = src[ i + 2 ];

        if ( val2 > max2 )
        {
            dst[ i + 2 ] = val2;
            ele_num2 += 1;
        }

        uint8_t max3 = dst[ i + 3 ];
        uint8_t val3 = src[ i + 3 ];

        if ( val3 > max3 )
        {
            dst[ i + 3 ] = val3;
            ele_num3 += 1;
        }

        i += 4;
    }

    return ele_num0 + ele_num1 + ele_num2 + ele_num3;
}



hll_ctx_t * hll_ctx_unSerialize( const uint8_t * src, int src_len, int * read_bytes )
{
    if ( NULL == src ) return NULL;

    int read_len = 0;

    // 校验：HLL_MAGIC  "HLL"
    if ( src[0] != 'H' || src[1] != 'L' || src[2] != 'L' ) return NULL;
    read_len += HLL_MAGIC_BYTES;

    uint8_t encoding = src[ read_len ];                             // 读取编码方式
    read_len += 1;

    hll_ctx_t * thiz = hll_ctx_create( encoding );                  // 重要:根据编码创建对象
    if ( NULL == thiz ) return NULL;

    read_len += varint_decode_uint32( src + read_len, (uint32_t *)&(thiz->ele_num) );

    if ( HLL_SPARSE == encoding )                                   // 稀疏编码方式
    {
        if ( src_len < ( read_len + HLL_REGI_MAX_BYTES ) )          // 避免读越界
            goto failed;

        memcpy( thiz->regi_arr, src + read_len, HLL_REGI_MAX_BYTES );
        read_len += HLL_REGI_MAX_BYTES;

        goto success;
    }

    // 稠密编码方式
    uint8_t * registers = thiz->registers;

    if ( thiz->ele_num < HLL_SERIAL_SPARSE_MIN )
    {
        uint8_t  val       = 0;                                         // 实际的值
        uint32_t num       = 0;                                         // 重复的次数
        int      write_idx = 0;

        while ( (val = src[ read_len ++ ]) != ( 1 << HLL_BITS ) )       // 一直循环到哨兵
        {
            read_len += varint_decode_uint32( src + read_len, &num );

            for ( uint32_t i = 0; i < num; i++ )
            {
                if ( write_idx < HLL_REGISTERS )
                    registers[ write_idx ] = val;                       // 写边界保护

                write_idx++;
            }

            if ( read_len >= src_len ) break;                           // 读边界保护
        }
    }
    else
    {
        if ( src_len < ( read_len + HLL_REGISTERS ) )                   // 读边界保护
            goto failed;

        memcpy( thiz->registers, src + read_len, HLL_REGISTERS );
        read_len += HLL_REGISTERS;
    }

success:
    *read_bytes = read_len;
    return thiz;

failed:
    hll_ctx_free( thiz );
    return NULL;
}



int hll_ctx_merge( hll_ctx_t * thiz, hll_ctx_t * for_merge )
{
    if ( NULL == thiz )      return -1;
    if ( NULL == for_merge ) return -1;

    if ( -1 == hll_ctx_sparse_to_dense( thiz ) )            // 转成稠密编码
        return -1;

    uint8_t encoding = for_merge->encoding;
    int     ele_num  = 0;

    if ( HLL_DENSE == encoding )
    {
        ele_num = hll_ctx_merge_registers( thiz->registers, for_merge->registers );
        thiz->ele_num += ele_num;

        return 0;
    }

    if ( HLL_SPARSE != encoding )  return -1;

    ele_num = hll_ctx_regi_arr_to_registers( thiz->registers, for_merge->regi_arr );
    thiz->ele_num += ele_num;

    return 0;
}



int hll_ctx_fast_merge( hll_ctx_t * thiz, const uint8_t * src, int src_len )
{
    if ( NULL == thiz || NULL == src ) return -1;

    int read_len = 0;
    int ele_num  = 0;

    // 校验：HLL_MAGIC  "HLL"
    if ( src[0] != 'H' || src[1] != 'L' || src[2] != 'L' ) return -1;
    read_len += HLL_MAGIC_BYTES;

    if ( -1 == hll_ctx_sparse_to_dense( thiz ) )                    // 转成稠密编码
        return -1;

    uint8_t * registers = thiz->registers;
    uint8_t   encoding  = src[ read_len ];                          // 读取编码方式
    read_len += 1;

    read_len += varint_decode_uint32( src + read_len, (uint32_t *)&(ele_num) );

    if ( HLL_SPARSE == encoding )                                   // 稀疏编码方式
    {
        if ( src_len < ( read_len + HLL_REGI_MAX_BYTES ) )          // 避免读越界
            goto failed;

        hll_regi_t * regi_arr = (hll_regi_t *)( src + read_len );   // 强行转换

        ele_num = hll_ctx_regi_arr_to_registers( thiz->registers, regi_arr );
        read_len += HLL_REGI_MAX_BYTES;

        goto success;
    }

    // 稠密编码方式
    if ( ele_num < HLL_SERIAL_SPARSE_MIN )
    {
        uint8_t  val       = 0;                                         // 实际的值
        uint32_t num       = 0;                                         // 重复的次数
        int      write_idx = 0;
                 ele_num   = 0;

        while ( (val = src[ read_len ++ ]) != ( 1 << HLL_BITS ) )       // 一直循环到哨兵
        {
            read_len += varint_decode_uint32( src + read_len, &num );

            for ( uint32_t i = 0; i < num; i++ )
            {
                if ( write_idx < HLL_REGISTERS && registers[ write_idx ] < val )
                {
                    registers[ write_idx ] = val;                       // 写边界保护
                    ele_num++;
                }

                write_idx++;
            }

            if ( read_len >= src_len ) break;                           // 读边界保护
        }
    }
    else
    {
        if ( src_len < ( read_len + HLL_REGISTERS ) )                   // 读边界保护
            goto failed;

        uint8_t * src_registers = (uint8_t *)(src + read_len );

        ele_num = hll_ctx_merge_registers( registers, src_registers );
        read_len += HLL_REGISTERS;
    }

success:
    thiz->ele_num += ele_num;
    return read_len;

failed:
    return -1;
}
