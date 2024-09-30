/**
    算法介绍链接： https://zhuanlan.zhihu.com/p/78798251  
   
    1.  Mini Batch K-Means算法
    是K-Means算法的一种优化变种，采用小规模的数据子集(每次训练使用的数据集是在训练算法的时候随机抽取的数据子集)减少计算时间，同时试图优化目标函数；
    可以减少K-Means算法的收敛时间 (性能提升3倍)，而且产生的结果效果只是略差于标准K-Means算法算法步骤如下：

    首先抽取部分数据集，使用K-Means算法构建出K个聚簇点的模型； 
    继续抽取训练数据集中的部分数据集样本数据，并将其添加到模型中，分配给距离最近的聚簇中心点；
    更新聚簇的中心点值；
    循环迭代第二步和第三步操作，直到中心点稳定或者达到迭代次数，停止计算操作

    2. ISODATA 方法，  采用这个
    全称是迭代自组织数据分析法。
    它解决了 K 的值需要预先人为的确定这一缺点。而当遇到高维度、海量的数据集时，人们往往很难准确地估计出 K 的大小。
    ISODATA 就是针对这个问题进行了改进，它的思想也很直观：当属于某个类别的样本数过少时把这个类别去除，当属于某个类别的样本数过多、
    分散程度较大时把这个类别分为两个子类别
 */

#pragma  once
#include <vector>
#include "dispatch_solver/problem_decomposition/comm_def.h"


DECOMPOSITION_NAMESPACE_BEGIN();


struct KmeansPoint 
{
    void  * obj;                // 用来分类的对象的指针
    float   x;
    float   y;
    int     cluster_id;         // id从1开始，0代表未正确归类

    KmeansPoint( void * obj, float x, float y, int id = 0 ) : obj( obj ), x( x ), y( y ), cluster_id( id )
    {}
};


struct KmeansCluster 
{
    float    x;
    float    y;
    int      id;                // id从1开始，0代表未正确归类
    int      num;               // 簇包含的点数量
    float    old_x;             // 上一次的位置，用来和 x 计算偏差
    float    old_y;

    KmeansCluster( float x, float y, int id = 0, int num = 0 ) : x(x), y( y ), id( id ), num(num), old_x(0), old_y(0)
    {}
};



/**
 * 1. K怎么选择？
 * 2. 初始中心点怎么确定？随便选吗？不同的初始点得到的最终聚类结果不同
 * 3. 点之间的距离用什么来定义？
 * 4. 所有点的均值（新的中心点）怎么算？
 * 5. 迭代终止条件， 如何判断聚类的质量？
 */
class Kmeans
{
public:
    Kmeans();
    ~Kmeans();

public:

    /**
     * points         待处理的点
     * cluster_num    要切除的簇的数量
     * max_iter_num   最多迭代的次数
     * min_errors     误差终止条件
     */
    std::vector<KmeansCluster> 
    plus_plus( std::vector<KmeansPoint>& points, int cluster_num, int max_iter_num, double min_errors, int rand_seed );


    /**
     * 当属于某个类别的样本数 < min_cluster_size 时把这个类别去除，
     * 当属于某个类别的样本数 > max_cluster_size 分散程度较大时把这个类别分为两个子类别
     * 当某个类别的分散程度   > max_sse 时把这个类别分为两个子类别
     */
    std::vector<KmeansCluster> 
    iso_data( std::vector<KmeansPoint>& points, 
              int       min_cluster_size, 
              int       max_cluster_size, 
              double    max_sse, 
              int       max_iter_num, 
              double    min_errors,
              int       rand_seed );


private:

    /** 初始化定义 簇的中心 */
    std::vector<KmeansCluster> 
    init_cluster_center_rand( std::vector<KmeansPoint>& points, int cluster_num );

    std::vector<KmeansCluster> 
    init_cluster_center_plus_plus( std::vector<KmeansPoint>& points, int cluster_num, int rand_seed );


    inline double 
    calc_distance(const KmeansPoint& point, const KmeansCluster& cluster );

    /** 进行聚类，把points 分配到 已经确定的 cluster中去 */
    int clustering( std::vector<KmeansPoint>& points, std::vector<KmeansCluster> & clusters );

    /** 校正 簇 的中心 */
    int refine_cluster_center( std::vector<KmeansPoint>& points, std::vector<KmeansCluster> & clusters );

    /** 对分簇的效果进行衡量 */
    double quantize( std::vector<KmeansCluster> & clusters );

private:

    int m_max_cluster_id = 1;                      // 簇的id从1开始编号
};


DECOMPOSITION_NAMESPACE_END();
