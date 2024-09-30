#pragma once

#include <vector>
#include <cmath>
#include "dispatch_solver/problem_decomposition/algo/comm_def.h"


ALGO_NAMESPACE_BEGIN();


using namespace std;


enum PointType 
{
    UNCLASSIFIED = 1,       // 未分类
    CORE_POINT   = 2,       // 核心对象
    BORDER_POINT = 3,       // 边界点
    NOISE        = 4        // 噪声
};


struct Point
{
    float       x, y;              // 表征点的向量数组
    int         clusterID;
    PointType   type;   
    int         id;

    Point(): x(0), y(0), clusterID(0), type(PointType::UNCLASSIFIED), id(0)
    {}
};

// 输入 特征向量的数组。   距离计算的回调函数



/**
 * DBSCAN算法的一些核心概念
 * 
 * 1. 领域距离：就是一个点 的周围半径R
 * 2. 核心对象： 在领域距离内，点的数量超过 minPts的
 * 3. 直接密度可达：在核心对象的，半径R以内的，称作 直接密度可达的点
 * 4. 密度可达：2个点，通过中间多层 直接密度可达的点进行传导，最终达到，则成为密度可达
 * 5. 密度相连：2个点，和中间的一个核心点 都是 密度可达的， 则称作2个点 密度相连
 * 6. 边界点：除了和上一个和自己直接密度可达的点外，没有找不到其他点了
 * 7. 噪声点：在R的半径内，没有任何其他的点
 */
class DBSCAN
{
public:    
    DBSCAN( int minPts, float eps, vector<Point> & points )
    {
        m_minPts    = minPts;
        m_epsilon   = eps;
        m_points    = points;
        m_pointSize = points.size();
    }
    ~DBSCAN(){}

    int run();

    vector<Point> & getPoints() { return m_points; }
    int getTotalPointSize() {   return m_pointSize; }
    int getMinClusterSize() {   return m_minPts;    }
    int getEpsilonSize()    {   return m_epsilon;   }

private:
    int expandCluster(Point & point, int clusterID);

    // 计算2个点之间的距离，后面这个要换成函数指针
    inline double calculateDistance( const Point& pointCore, const Point& pointTarget, double epsilon );

    vector<int> buildAllRange(Point & point);

private:    
    vector<Point>   m_points;               // 所有点的数组

    // 每个点对应的半径范围内的 其他点的id 数组
    vector< vector< int > >  m_pointRanges;               

    int             m_pointSize;            // 总的点的数量
    int             m_minPts;               // 直接密度可达 的点的最小数量
    double          m_epsilon;              // 边界半径
};



ALGO_NAMESPACE_END();
