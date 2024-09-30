#include <cfloat>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>     
#include <time.h>       
#include "dispatch_solver/problem_decomposition/algo/kmeans/kmeans.h"


DECOMPOSITION_NAMESPACE_BEGIN();


Kmeans::Kmeans(){}
Kmeans::~Kmeans(){}

double 
Kmeans::quantize( std::vector<KmeansCluster> & clusters )
{
    double result = 0;
    int    cluster_num = clusters.size();

    for ( int j = 0; j < cluster_num; j++ )
    {   
        KmeansCluster & cluster = clusters.at( j );

        result += sqrt( pow( cluster.x - cluster.old_x, 2 ) + pow( cluster.y - cluster.old_y, 2 ) );
        //result += (abs( cluster.x - cluster.old_x ) + abs( cluster.y - cluster.old_y ));
    }

    return result;
}




/**
K-Means距离计算优化elkan K-Means,  备注：未实现

elkan K-Means利用了两边之和大于等于第三边,以及两边之差小于第三边的三角形性质，来减少距离的计算。

第一种规律是: 对于一个样本点𝑥和两个质心𝜇𝑗1,𝜇𝑗2。如果我们预先计算出了这两个质心之间的距离𝐷(𝑗1,𝑗2)，
            则如果计算发现2𝐷(𝑥,𝑗1)≤𝐷(𝑗1,𝑗2),我们立即就可以知道𝐷(𝑥,𝑗1)≤𝐷(𝑥,𝑗2)。此时我们不需要再计算𝐷(𝑥,𝑗2),也就是说省了一步距离计算。
第二种规律是: 对于一个样本点𝑥和两个质心𝜇𝑗1,𝜇𝑗2。我们可以得到𝐷(𝑥,𝑗2)≥𝑚𝑎𝑥{0,𝐷(𝑥,𝑗1)−𝐷(𝑗1,𝑗2)}。这个从三角形的性质也很容易得到。

利用上边的两个规律，elkan K-Means比起传统的K-Means迭代速度有很大的提高
 */
inline double 
Kmeans::calc_distance(const KmeansPoint& point, const KmeansCluster& cluster )
{
    // 欧式距离
    // float  dx = point.x - cluster.x;
    // float  dy = point.y - cluster.y;

    // double distance = pow( dx, 2 ) + pow( dy, 2 );

    // distance = sqrt( distance );
    
    
    // 曼哈顿距离
    float  dx = point.x - cluster.x;
    float  dy = point.y - cluster.y;

    double distance = abs( dx ) + abs( dy );
    

    return distance;
}



/** 重新进行聚类 */
int 
Kmeans::clustering( std::vector<KmeansPoint>   & points, 
                    std::vector<KmeansCluster> & clusters )
{
    int cluster_num = clusters.size();
    int point_num   = points.size();    

    for ( int i = 0; i < point_num; i++ )
    {
        KmeansPoint & point = points.at( i );

        double min_distance    = DBL_MAX;
        int    min_cluster_id  = 0;
        // int    min_cluster_idx = 0;

        for ( int j = 0; j < cluster_num; j++ )
        {   
            KmeansCluster & cluster = clusters.at( j );

            double distance = calc_distance( point, cluster );

            if ( distance < min_distance )
            {
                min_distance    = distance;
                min_cluster_id  = cluster.id;
                // min_cluster_idx = j;
            }
        }

        point.cluster_id = min_cluster_id;  
    }

    return 0;
}




/** 
 * 校正 簇 的中心
 */
int 
Kmeans::refine_cluster_center( std::vector<KmeansPoint>& points, 
                               std::vector<KmeansCluster> & clusters )
{
    int cluster_num = clusters.size();
    int point_num   = points.size();

    for ( int i = 0; i < cluster_num; i++ )
    {
        KmeansCluster & cluster = clusters.at( i );

        int cluster_id = cluster.id;

        int     cluster_point_num = 0;
        double  total_x = 0;
        double  total_y = 0;

        for ( int j = 0; j < point_num; j++ )
        {
            KmeansPoint & point = points.at( j );

            if ( point.cluster_id != cluster_id )
                continue;

            total_x += point.x;
            total_y += point.y;

            cluster_point_num++;
        }

        cluster.old_x = cluster.x;
        cluster.old_y = cluster.y;
        cluster.num   = cluster_point_num;

        cluster.x = (total_x / cluster_point_num);
        cluster.y = (total_y / cluster_point_num);
    }

    return 0;
}


// std::vector<KmeansCluster> 
// Kmeans::init_cluster_center_rand( std::vector<KmeansPoint>& points, 
//                                   int cluster_num )
// {
//     std::vector<KmeansCluster> clusters;
//     int  point_num = points.size();             

//     srand (time(NULL));
    
//     for ( int i = 0; i < cluster_num; i++ )
//     {
//         KmeansPoint   & point   = points.at( rand() % point_num );
//         KmeansCluster   cluster = KmeansCluster( point.x, point.y, m_max_cluster_id );

//         m_max_cluster_id++;
//         clusters.push_back( cluster );
//     }

//     return clusters;
// }


std::vector<KmeansCluster> 
Kmeans::init_cluster_center_plus_plus(  std::vector<KmeansPoint>& points, 
                                        int cluster_num,
                                        int rand_seed )
{
    std::vector<KmeansCluster> clusters;
    int  point_num = points.size();             

    srand (rand_seed);
    int select_point_idx = rand() % point_num;          // 先随机选择一个 

    std::vector<bool> point_idx_bitmap( point_num, false );
    point_idx_bitmap[ select_point_idx ] = true;

    KmeansPoint   & point    = points.at( select_point_idx );
    KmeansCluster   cluster1 = KmeansCluster( point.x, point.y, m_max_cluster_id );
    
    m_max_cluster_id++;
    clusters.push_back( cluster1 );

    while ( (int)clusters.size() < cluster_num )
    {
        int     max_point_idx = 0;
        double  max_distance  = 0;

        // 选出最远的1个 point
        for ( int i = 0; i < point_num; i++ )
        {
            if ( true ==  point_idx_bitmap[ i ])
                continue;

            KmeansPoint & point = points.at( i );
            double   distance = 0;
            
            // 求点 和所有的 簇心的距离之和
            for (KmeansCluster & tmp_cluster : clusters )
            {
                distance += calc_distance( point, tmp_cluster );
            }

            if ( distance > max_distance )
            {
                max_distance  = distance;
                max_point_idx = i;
            }
        }

        point_idx_bitmap[ max_point_idx ] = true;

        KmeansPoint   & point = points.at( max_point_idx );
        KmeansCluster   cluster2 = KmeansCluster( point.x, point.y, m_max_cluster_id );

        m_max_cluster_id++;
        clusters.push_back( cluster2 );
    }
 
    return clusters;
}




/**
 * points ： 输入的点的数组， 簇 会通过 cluster_id 传递出去，是否合理回头再弄
 * cluster_num ： 希望切分出的簇的个数
 * max_iter_num ：最大迭代的次数
 */
std::vector<KmeansCluster> 
Kmeans::plus_plus(  std::vector<KmeansPoint> & points, 
                    int     cluster_num, 
                    int     max_iter_num,
                    double  min_errors,
                    int     rand_seed )
{
    std::vector<KmeansCluster> clusters;

    int  point_num = points.size();
    if ( point_num <= 0 ) return clusters;

    // 点数 没有 聚类数多
    if ( point_num < cluster_num )
    {
        for ( int i = 0; i < point_num; i++ )
        {
            KmeansPoint   & point   = points.at( i );
            KmeansCluster   cluster = KmeansCluster( point.x, point.y, m_max_cluster_id, 1 );

            point.cluster_id = m_max_cluster_id;

            m_max_cluster_id++;
            clusters.push_back( cluster );
        }

        return clusters;
    }

    // 初始化种子点
    clusters = init_cluster_center_plus_plus( points, cluster_num, rand_seed ); 

    // 开始进行迭代
    int  iter_num = 0;
    do {
        // 聚类
        clustering( points, clusters );

        // 重新计算种子点的坐标. 顺便统计了一下 每个簇的点数
        refine_cluster_center( points, clusters );

        // 根据聚类质量（主要根据位移的距离），进行终止判断
        if ( quantize( clusters ) < min_errors )
            break;

        //printf( " iter_num: %d\n", iter_num );
        iter_num++;
    } while ( iter_num < max_iter_num );
    
    return clusters;
}




/**
 * 当属于某个类别的样本数 < min_cluster_size 时把这个类别去除，
 * 当属于某个类别的样本数 > max_cluster_size 分散程度较大时把这个类别分为两个子类别
 * 当某个类别的分散程度   > max_sse 时把这个类别分为两个子类别
 */
std::vector<KmeansCluster> 
Kmeans::iso_data( std::vector<KmeansPoint>& points, 
                  int min_cluster_size, 
                  int max_cluster_size, 
                  double max_sse, 
                  int max_iter_num, 
                  double min_errors,
                  int rand_seed )
{
    int   point_num   = points.size();
    int   cluster_num = 1; //(point_num / max_cluster_size) + 1;

    std::vector<KmeansCluster> clusters = plus_plus( points, cluster_num, max_iter_num, min_errors, rand_seed );

    do 
    {
        cluster_num = clusters.size();

        for ( int k = 0; k < cluster_num; k++ )
        {   
            KmeansCluster & cluster = clusters.at( k );

            // 进行拆分, 判断簇中点的数量
            if ( cluster.num > max_cluster_size )
            {
                int cluster_id = cluster.id;
                std::vector<KmeansPoint> sub_points;
                
                for ( int i = 0; i < point_num; i++ )
                {
                    KmeansPoint & point = points.at( i );

                    if ( point.cluster_id != cluster_id )
                        continue;

                    // 请注意：位置一定保持固定，千万不要改
                    sub_points.push_back( point );
                }

                // 1个簇  切开成2个簇
                std::vector<KmeansCluster> sub_clusters = plus_plus( sub_points, 2, max_iter_num, min_errors, rand_seed );
                if ( sub_clusters.size() != 2 ) continue;

                KmeansCluster & sub_clus0 = sub_clusters.at( 0 );
                KmeansCluster & sub_clus1 = sub_clusters.at( 1 );                

                if ( sub_clus0.num > 0 ) cluster = sub_clus0;                 // 覆盖第一个
                if ( sub_clus1.num > 0 ) clusters.push_back( sub_clus1 );     // 把第二个存起来

                // 再把 点 也设置回来
                int  sub_point_index = 0;
                for ( int i = 0; i < point_num; i++ )
                {
                    KmeansPoint & point = points.at( i );

                    if ( point.cluster_id != cluster_id )
                        continue;

                    point.cluster_id = sub_points.at( sub_point_index ).cluster_id;
                    sub_point_index++;
                }
            }
        }

        // 不再变化了
        if ( cluster_num == (int)clusters.size() ) break;
    } while ( true );

    // 开始进行迭代
    int  iter_num = 0;
    do {
        // 聚类
        clustering( points, clusters );

        // 重新计算种子点的坐标. 顺便统计了一下 每个簇的点数
        refine_cluster_center( points, clusters );

        // 根据聚类质量（主要根据位移的距离），进行终止判断
        if ( quantize( clusters ) < min_errors )
            break;

        //printf( " iter_num: %d\n", iter_num );
        iter_num++;
    } while ( iter_num < max_iter_num );

    return clusters;
}



DECOMPOSITION_NAMESPACE_END();
