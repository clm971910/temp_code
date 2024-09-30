#include <stdio.h>
#include <climits>
#include <cfloat>

#include "dbscan.h"


ALGO_NAMESPACE_BEGIN();


int DBSCAN::run()
{
    // 先统一编个号，给个自增id，目前这个id和他在vector中的index是一样的
    int index = 0;
    for ( Point & point : m_points )
    {
        point.id = index;
        index++;
    }

    // 先准备足够的空间
    m_pointRanges.reserve( m_points.size() );

    // 循环给每个点建立 他的range
    for ( Point & point : m_points )
    {
        vector<int> range = buildAllRange( point );

        int num = range.size();
        if ( num <= 0 )        point.type = PointType::NOISE;
        if ( num >= m_minPts ) point.type = PointType::CORE_POINT;

        // 默认都是 UNCLASSIFIED,  下标的位置要严格对应在 point.id
        // m_pointRanges[ point.id ] = range;
        // m_pointRanges.assign( point.id, range );
        m_pointRanges.push_back( range );
    }

    // 处理好每个点的类型
    for ( Point & point : m_points )
    {
        if ( point.type != PointType::UNCLASSIFIED )
            continue;

        vector<int> & range = m_pointRanges.at( point.id );

        // 在 1  m_minPts 之间的 都还是 UNCLASSIFIED
        if ( 1 == range.size() )
        {
            Point & to_point = m_points.at( range.at( 0 ) );

            if ( to_point.type == PointType::CORE_POINT )   point.type = PointType::BORDER_POINT;
            if ( to_point.type == PointType::NOISE )        point.type = PointType::UNCLASSIFIED;   // 这是不可能出现的
            if ( to_point.type == PointType::BORDER_POINT ) point.type = PointType::BORDER_POINT;
            if ( to_point.type == PointType::UNCLASSIFIED ) point.type = PointType::BORDER_POINT;             
        }
    }

    // 切分成多个 簇 
    int clusterID = 1;

    for ( Point & point : m_points )
    {
        if ( point.type != PointType::CORE_POINT )
            continue;

        if ( expandCluster( point, clusterID ) >= 0 )
        {
            clusterID += 1;
        }
    }

    return 0;
}



vector<int>  DBSCAN::buildAllRange(Point & point)
{
    vector<int> range;

    // 一定是从前往后加的，所有里面的id是有序的，但不是严格递增的
    for ( Point & to_point : m_points )
    {
        if ( point.id == to_point.id )  continue;       // 同一个点，就不要放进去了

        if ( calculateDistance( point, to_point, m_epsilon ) <= m_epsilon )
        {
            range.push_back( to_point.id );
        }
    }

    return range;
}



// 从核心点出发，进行传染的探索，给 相关点都标上同一个clusterID
int DBSCAN::expandCluster( Point & point, int clusterID )
{    
    if ( point.type != PointType::CORE_POINT )
        return -1;
    
    if ( point.clusterID > 0 )
        return -1;

    point.clusterID = clusterID;

    // 注意，这里的数组 是复制出来的，因为需要改.   直接密度可达点数组
    vector<int> clusterSeeds = m_pointRanges.at( point.id );

    // 开始循环，进行 直接密度可达的传导，找密度可达
    for ( size_t i = 0; i < clusterSeeds.size(); i++ )
    {
        // 直接密度可达点
        Point & seed_point = m_points.at( clusterSeeds[i] );

        if ( 0 == seed_point.clusterID ) 
            seed_point.clusterID = clusterID;

        // 下一级的关联点.  密度可达点数组
        vector<int> & clusterNeighors = m_pointRanges.at( seed_point.id );

        for ( int pointId_neighor : clusterNeighors )
        {
            // 密度可达点
            Point & neighor_point = m_points.at( pointId_neighor );

            if ( 0 != neighor_point.clusterID )  
                continue;

            neighor_point.clusterID = clusterID;

            // 只有core点 才能进行下一级的探索
            if ( neighor_point.type == PointType::CORE_POINT )
                clusterSeeds.push_back( neighor_point.id );
        }
    }

    return 0;
}



inline double 
DBSCAN::calculateDistance(const Point& pointCore, const Point& pointTarget, double epsilon )
{
    // 这里如果做一个cache能减少一半运算，因为额外存取需要耗时，所以不一定省时间，以后再说
    float  dx = pointCore.x - pointTarget.x;
    float  dy = pointCore.y - pointTarget.y;

    if ( dx >= epsilon ) return DBL_MAX;
    if ( dy >= epsilon ) return DBL_MAX;

    double distance = pow( dx, 2 ) + pow( dy, 2 );

    distance = sqrt( distance );

    //printf("%.4f\n", distance );
    return distance;
}



ALGO_NAMESPACE_END();
